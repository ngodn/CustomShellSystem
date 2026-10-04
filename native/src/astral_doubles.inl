// Owned visuals only. Native AI, combat animation, sockets and collision remain in place.
namespace {
struct AstralVisualGroup {
    AstralObservation observed;
    uint64_t activation=0, prepared_at=0;
    AstralMaterials materials;
    std::vector<std::unique_ptr<AstralVisualMesh>> visuals;
    AstralMaterialBindings bindings;
    AstralNativeRenderLease native;
    bool visible=false;

    bool release() noexcept {
        // Destroy children before their pose parent. Restore native visibility even
        // if a component refuses destruction, leaving the failed group for retry.
        bool result=true;
        for(auto it=visuals.rbegin();it!=visuals.rend();++it) result=(*it)->release() && result;
        result=bindings.restore() && result;
        result=native.restore() && result;
        if(result) { visuals.clear();materials.release();visible=false; }
        return result;
    }
    ~AstralVisualGroup() { release(); }

    void prepare(const AstralEntry& entry,const AstralAppearanceSource& source,
                 const AstralMaterialAdapters& adapters,UObject* pose,uint64_t now) {
        observed=entry.observed;activation=entry.activation;prepared_at=now;
        auto* actor=astral_resolve(observed.actor);
        auto* parent=astral_resolve(observed.mesh);
        auto* mid=astral_resolve(observed.native_mid);
        if(!actor || !parent || !mid || source.components.empty())
            throw std::runtime_error("Astral double expired before visual preparation");
        materials.prepare(actor,mid,adapters.inputs(),observed.kind!=AstralKind::faithful);
        std::vector<AstralMaterialBinding> pending;
        size_t material_index=0;
        for(size_t i=0;i<source.components.size();++i) {
            const auto& part=source.components[i];
            auto visual=std::make_unique<AstralVisualMesh>();
            auto* component_parent=i?visuals.front()->component():parent;
            // The body uses the native combat mesh's component space so its
            // copied pose stays aligned with the double's native weapon sockets.
            const AstralVisualTransform relative=i?
                AstralVisualTransform{part.location,part.rotation,part.scale}:AstralVisualTransform{};
            visuals.push_back(std::move(visual));
            visuals.back()->prepare(component_parent,part,pose,relative);
            AstralMaterialBinding binding;
            binding.component=visuals.back()->component();binding.mesh=part.mesh.Get();
            for(size_t slot=0;slot<part.materials.size();++slot) {
                binding.materials.push_back(materials.material(material_index++));
                binding.overlays.push_back(part.overlays.at(slot).Get()?materials.material(material_index++):nullptr);
            }
            pending.push_back(std::move(binding));
        }
        if(material_index!=materials.size()) throw std::runtime_error("Astral material slot plan differs from captured appearance");
        bindings.bind(actor,pending);
    }
    bool sync(uint64_t now) {
        auto* actor=astral_resolve(observed.actor);
        auto* component=astral_resolve(observed.component);
        if(!actor || !component || !astral_bool(component,L"bInitialized") ||
           !astral_bool(component,L"bCharacterEnabled") || astral_bool(actor,L"bHidden") ||
           astral_object(component,L"MID_Astral")!=astral_resolve(observed.native_mid) ||
           astral_object(component,L"MySkeletalMesh")!=astral_resolve(observed.mesh)) return false;
        if(!materials.sync_opacity()) return false;
        if(!visible && now>prepared_at) {
            if(!bindings.intact()) return false;
            for(const auto& visual:visuals) if(!visual->intact()) return false;
            native.acquire(astral_resolve(observed.mesh));
            for(const auto& visual:visuals) visual->show(true);
            visible=true;
        }
        return true;
    }
    bool intact() const {
        if(!bindings.intact() || (visible && !native.intact())) return false;
        for(const auto& visual:visuals) if(!visual->intact()) return false;
        return true;
    }
};
}

struct AstralDoubles::Impl {
    AstralLifecycle lifecycle;
    AstralContext context;
    std::unique_ptr<AstralAppearanceSource> source;
    AstralMaterialAdapters adapters;
    AssetLoadRoots retained;
    WeakObject pose;
    std::vector<std::unique_ptr<AstralVisualGroup>> groups;
    struct Failed { AstralIdentity actor;uint64_t activation; };
    std::vector<Failed> failed;
    uint64_t next_poll=0, player_revision=0, appearance_revision=0;
    bool source_attempted=false;
    std::string error;
    size_t prepared=0, removed=0;

    void record_failure(AstralIdentity actor,uint64_t activation) {
        if(std::none_of(failed.begin(),failed.end(),[&](const auto& value) {
            return value.actor==actor && value.activation==activation;
        })) {
            if(failed.size()>=AstralLifecycle::capacity) failed.erase(failed.begin());
            failed.push_back({actor,activation});
        }
    }

    bool release_groups() noexcept {
        bool clean=true;
        for(auto it=groups.begin();it!=groups.end();) {
            if((*it)->release()) { it=groups.erase(it);++removed; }
            else { clean=false;++it; }
        }
        return clean;
    }
    bool clear() noexcept {
        if(!release_groups()) return false;
        source.reset();adapters.release();retained.release();pose=WeakObject{};
        failed.clear();lifecycle.clear();context={};next_poll=0;
        source_attempted=false;player_revision=appearance_revision=0;
        return true;
    }
    void prepare_source(Appearance& appearance) {
        adapters.release();
        source=appearance.astral_source();
        if(!source) return;
        source_attempted=true;
        if(astral_identity(source->pawn.Get())!=context.player)
            throw std::runtime_error("Astral source and spawner have different owners");
        std::vector<UObject*> material_sources;
        for(const auto& part:source->components) {
            if(part.materials.size()!=part.overlays.size())
                throw std::runtime_error("Astral source overlay count differs");
            for(size_t slot=0;slot<part.materials.size();++slot) {
                auto* material=part.materials[slot].Get();
                if(!material) throw std::runtime_error("Astral source material expired");
                material_sources.push_back(material);
                if(auto* overlay=part.overlays[slot].Get()) material_sources.push_back(overlay);
                else if(part.overlays[slot].ObjectIndex>=0) throw std::runtime_error("Astral source overlay expired");
            }
        }
        adapters.prepare(source->pawn.Get(),material_sources);
        if(!pose.Get()) {
            auto* value=load("/Game/CSS/SharedAssets/Astral/ABP_CopyPose.ABP_CopyPose_C");
            astral_retain(retained,value);pose=value;
        }
    }
    void update(void* engine,Appearance& appearance,bool enabled,uint64_t now) {
        if(!enabled || !astral_shell(appearance.shell)) {
            if(!clear()) throw std::runtime_error("Astral cleanup is incomplete");
            return;
        }
        // Fade updates touch only active private MIDs; discovery is bounded to
        // the player's own spawner and runs at 20 Hz.
        for(auto it=groups.begin();it!=groups.end();) {
            bool valid=false;
            try { valid=(*it)->sync(now); }
            catch(const std::exception& problem) { error=problem.what(); }
            if(!valid) {
                record_failure((*it)->observed.actor,(*it)->activation);
                if(!(*it)->release()) throw std::runtime_error("Astral inactive visual cleanup is incomplete");
                it=groups.erase(it);++removed;
            } else ++it;
        }
        if(now<next_poll) return;
        next_poll=now+50;
        const auto snapshot=astral_snapshot(engine);
        const bool changed=context!=snapshot.context || player_revision!=appearance.player_revision ||
                           appearance_revision!=appearance.appearance_revision;
        if(changed) {
            if(!clear()) throw std::runtime_error("Astral appearance-change cleanup is incomplete");
            context=snapshot.context;player_revision=appearance.player_revision;
            appearance_revision=appearance.appearance_revision;next_poll=now+50;error.clear();
        }
        if(!context.valid()) return;
        const auto changes=lifecycle.observe(context,snapshot.view());
        if(!changes.accepted) throw std::runtime_error("Astral lifecycle snapshot rejected");
        const auto entries=lifecycle.entries();
        auto current=[&](AstralIdentity actor,uint64_t activation) {
            return std::any_of(entries.begin(),entries.end(),[&](const auto& entry) {
                return entry.observed.actor==actor && entry.activation==activation && entry.observed.active();
            });
        };
        std::erase_if(failed,[&](const auto& value){return !current(value.actor,value.activation);});
        for(auto it=groups.begin();it!=groups.end();) {
            if(!current((*it)->observed.actor,(*it)->activation) || !(*it)->intact()) {
                if(!(*it)->release()) throw std::runtime_error("Astral rebound cleanup is incomplete");
                it=groups.erase(it);++removed;
            } else ++it;
        }
        bool refreshed=!source_attempted;
        if(refreshed) {
            try { prepare_source(appearance); }
            catch(const std::exception& problem) {
                error=problem.what();source_attempted=true;
                source.reset();adapters.release();pose=WeakObject{};retained.release();
            }
        }
        if(!source || !pose.Get()) return;
        for(const auto& entry:entries) {
            if(!entry.observed.active() ||
               std::any_of(groups.begin(),groups.end(),[&](const auto& group){return group->observed.actor==entry.observed.actor;}) ||
               std::any_of(failed.begin(),failed.end(),[&](const auto& value){return value.actor==entry.observed.actor && value.activation==entry.activation;})) continue;
            // Source visibility and physics flags may change without a CSS
            // edit (for example, leaving Inventory). Capture them at activation.
            if(!refreshed) {
                refreshed=true;
                try { prepare_source(appearance); }
                catch(const std::exception& problem) {
                    error=problem.what();source.reset();adapters.release();
                    break;
                }
                if(!source) break;
            }
            auto group=std::make_unique<AstralVisualGroup>();
            groups.push_back(std::move(group));
            try { groups.back()->prepare(entry,*source,adapters,pose.Get(),now);++prepared; }
            catch(const std::exception& problem) {
                error=problem.what();record_failure(entry.observed.actor,entry.activation);
                if(!groups.back()->release()) throw std::runtime_error("Astral failed preparation cleanup is incomplete");
                groups.pop_back();
            }
        }
    }
};
AstralDoubles::AstralDoubles():impl_(std::make_unique<Impl>()) {}
AstralDoubles::~AstralDoubles() { clear(); }
void AstralDoubles::update(void* engine,Appearance& appearance,bool enabled,uint64_t now) {
    try { impl_->update(engine,appearance,enabled,now); }
    catch(const std::exception& problem) {
        impl_->error=problem.what();impl_->release_groups();impl_->next_poll=now+1000;
    }
}
bool AstralDoubles::clear() noexcept { return impl_->clear(); }
Json AstralDoubles::diagnostics() const {
    return {{"active",impl_->groups.size()},{"prepared",impl_->prepared},{"removed",impl_->removed},
        {"source_ready",impl_->source!=nullptr && impl_->pose.Get()!=nullptr},
        {"fallbacks",impl_->failed.size()},{"error",impl_->error}};
}

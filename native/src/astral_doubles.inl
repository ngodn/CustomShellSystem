// Owned visuals only. Native AI, combat animation, sockets and collision remain in place.
namespace {
bool astral_same_visual_source(const AstralComponentSource& a,const AstralComponentSource& b) {
    const auto& x=a.physics;const auto& y=b.physics;
    return a.component.Get() && a.component.Get()==b.component.Get() &&
        a.mesh.Get() && a.mesh.Get()==b.mesh.Get() && a.item==b.item &&
        a.visible==b.visible && a.hidden_in_game==b.hidden_in_game && a.leader_pose==b.leader_pose &&
        a.materials.size()==b.materials.size() && a.overlays.size()==b.overlays.size() &&
        a.hidden_by_lod==b.hidden_by_lod && a.morphs==b.morphs &&
        a.location==b.location && a.rotation==b.rotation && a.scale==b.scale &&
        x.animation_class.Get()==y.animation_class.Get() &&
        x.post_process_disabled==y.post_process_disabled && x.cloth_disabled==y.cloth_disabled &&
        x.rigid_body_disabled==y.rigid_body_disabled && x.springs==y.springs &&
        x.dynamics==y.dynamics && x.rig==y.rig && x.body_rig==y.body_rig && x.geometry==y.geometry;
}
struct AstralVisualGroup {
    AstralObservation observed;
    uint64_t activation=0, prepared_at=0;
    AstralMaterials materials;
    std::vector<std::unique_ptr<AstralVisualMesh>> visuals;
    AstralMaterialBindings bindings;
    AstralNativeRenderLease native;
    bool visible=false, native_hidden=false, parked=false;
    uint64_t parked_at=0;
    std::vector<AstralComponentSource> captured;

    bool release() noexcept {
#ifdef CSS_INVENTORY_DEV
        AstralTiming timing(AstralPhase::release);
#endif
        // Restore bindings while their components are still alive.
        bool result=bindings.restore();
        // Destroy children before their pose parent. Restore native visibility even
        // if a component refuses destruction, leaving the failed group for retry.
        for(auto it=visuals.rbegin();it!=visuals.rend();++it) result=(*it)->release() && result;
        result=native.restore() && result;
        if(result) { visuals.clear();captured.clear();materials.release();visible=native_hidden=parked=false; }
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
        prepare_visuals(source,pose);
    }
    void prepare_visuals(const AstralAppearanceSource& source,UObject* pose) {
        auto* actor=astral_resolve(observed.actor);
        auto* parent=astral_resolve(observed.mesh);
        if(!actor || !parent) throw std::runtime_error("Astral double expired before pose preparation");
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
        }
        bind_materials(source);
        captured=source.components;
    }
    void bind_materials(const AstralAppearanceSource& source) {
        if(visuals.size()!=source.components.size()) throw std::runtime_error("Astral cached component count differs");
        std::vector<AstralMaterialBinding> pending;
        size_t material_index=0;
        for(size_t i=0;i<source.components.size();++i) {
            const auto& part=source.components[i];
            AstralMaterialBinding binding;
            binding.component=visuals[i]->component();binding.mesh=part.mesh.Get();
            for(size_t slot=0;slot<part.materials.size();++slot) {
                binding.materials.push_back(materials.material(material_index++));
                binding.overlays.push_back(part.overlays.at(slot).Get()?materials.material(material_index++):nullptr);
            }
            pending.push_back(std::move(binding));
        }
        if(material_index!=materials.size()) throw std::runtime_error("Astral material slot plan differs from captured appearance");
        bindings.bind(astral_resolve(observed.actor),pending);
    }
    bool can_resume(const AstralAppearanceSource& source) const {
        if(!parked || captured.size()!=source.components.size() || visuals.size()!=captured.size()) return false;
        for(size_t i=0;i<captured.size();++i)
            if(!astral_same_visual_source(captured[i],source.components[i]) ||
               !visuals[i]->intact() || !visuals[i]->pose_source_intact()) return false;
        return true;
    }
    void park(uint64_t now) {
        for(size_t i=0;i<visuals.size();++i) visuals[i]->set_paused(true,captured.at(i).physics);
        if(!native.restore() || !bindings.restore()) throw std::runtime_error("Astral cached visual restoration failed");
        materials.release();visible=native_hidden=false;parked=true;parked_at=now;
    }
    void resume(const AstralEntry& entry,const AstralAppearanceSource& source,
                const AstralMaterialAdapters& adapters,uint64_t now) {
        observed=entry.observed;activation=entry.activation;prepared_at=now;
        materials.prepare(astral_resolve(observed.actor),astral_resolve(observed.native_mid),
                          adapters.inputs(),observed.kind!=AstralKind::faithful);
        bind_materials(source);
        for(size_t i=0;i<visuals.size();++i) visuals[i]->set_paused(false,source.components[i].physics);
        parked=false;
    }
    void rebind_pose(const AstralAppearanceSource& source,UObject* pose,uint64_t now) {
        if(!bindings.restore()) throw std::runtime_error("Astral pose binding cleanup is incomplete");
        bool clean=true;
        for(auto it=visuals.rbegin();it!=visuals.rend();++it) clean=(*it)->release() && clean;
        if(!clean) throw std::runtime_error("Astral pose visual cleanup is incomplete");
        visuals.clear();visible=false;prepared_at=now;
        // The native ability changed its pose leader, not its material state.
        // Keep the existing MIDs and fade binding while replacing private poses.
        prepare_visuals(source,pose);
    }
    enum class Sync { kept, inactive, rebound, failed };
    Sync sync(uint64_t now) {
        auto* actor=astral_resolve(observed.actor);
        auto* component=astral_resolve(observed.component);
        if(!actor || !component || !astral_bool(component,L"bInitialized") ||
           !astral_bool(component,L"bCharacterEnabled") || astral_bool(actor,L"bHidden")) return Sync::inactive;
        if(astral_object(component,L"MID_Astral")!=astral_resolve(observed.native_mid) ||
           astral_object(component,L"MySkeletalMesh")!=astral_resolve(observed.mesh)) return Sync::failed;
        for(const auto& visual:visuals) if(!visual->pose_source_intact()) return Sync::rebound;
        if(!materials.sync_opacity()) return Sync::failed;
        if(!visible && now>prepared_at) {
            if(!bindings.intact()) return Sync::failed;
            for(const auto& visual:visuals) if(!visual->intact()) return Sync::failed;
            if(!native_hidden) { native.acquire(astral_resolve(observed.mesh));native_hidden=true; }
            for(const auto& visual:visuals) visual->show(true);
            visible=true;
        }
        return Sync::kept;
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
    uint64_t next_poll=0, source_retry_at=0, player_revision=0, appearance_revision=0;
    bool source_attempted=false;
    std::string error;
    size_t prepared=0, removed=0, pose_rebinds=0, reused=0;

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
        failed.clear();lifecycle.clear();context={};next_poll=source_retry_at=0;
        source_attempted=false;player_revision=appearance_revision=0;
        return true;
    }
    void prepare_source(Appearance& appearance,uint64_t now) {
        adapters.release();
        source_retry_at=now+1000;
        source=appearance.astral_source();
        if(!source) { source_attempted=false;return; }
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
    void update(void* engine,Appearance& appearance,bool enabled,const DoublesOptions& options,uint64_t now) {
        if(!enabled || !astral_shell(appearance.shell) || (!options.faithful_css && !options.stray_css)) {
            if(!clear()) throw std::runtime_error("Astral cleanup is incomplete");
            return;
        }
        // Fade updates touch only active private MIDs; discovery is bounded to
        // the player's own spawner and runs at 20 Hz.
        for(auto it=groups.begin();it!=groups.end();) {
            if(!astral_kind_enabled((*it)->observed.kind,options.faithful_css,options.stray_css)) {
                if(!(*it)->release()) throw std::runtime_error("Astral default-mode cleanup is incomplete");
                it=groups.erase(it);++removed;continue;
            }
            if((*it)->parked) { ++it;continue; }
            auto result=AstralVisualGroup::Sync::failed;
            try { result=(*it)->sync(now); }
            catch(const std::exception& problem) { error=problem.what(); }
            if(result==AstralVisualGroup::Sync::rebound) {
                try {
                    if(!source || !pose.Get()) throw std::runtime_error("Astral pose source is unavailable");
                    (*it)->rebind_pose(*source,pose.Get(),now);
                    ++pose_rebinds;
                    result=AstralVisualGroup::Sync::kept;
                } catch(const std::exception& problem) {
                    error=problem.what();result=AstralVisualGroup::Sync::failed;
                }
            }
            if(result!=AstralVisualGroup::Sync::kept) {
                if(result==AstralVisualGroup::Sync::inactive) {
                    lifecycle.deactivate((*it)->observed.actor);
                    auto* component=astral_resolve((*it)->observed.component);
                    if(component && astral_bool(component,L"bIsCached") && (*it)->intact()) {
                        try { (*it)->park(now);++it;continue; }
                        catch(const std::exception& problem) { error=problem.what(); }
                    }
                } else record_failure((*it)->observed.actor,(*it)->activation);
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
            bool keep=false;
            if((*it)->parked) {
                for(const auto& entry:entries)
                    if(astral_cached_visual_matches((*it)->observed,entry.observed,(*it)->parked_at,now)) {
                        keep=true;break;
                    }
            } else keep=current((*it)->observed.actor,(*it)->activation) && (*it)->intact();
            if(!keep) {
                if(!(*it)->release()) throw std::runtime_error("Astral rebound cleanup is incomplete");
                it=groups.erase(it);++removed;
            } else ++it;
        }
        const bool activated=std::any_of(changes.view().begin(),changes.view().end(),[](const auto& event) {
            return event.kind==AstralEventKind::activated || event.kind==AstralEventKind::rebound;
        });
        bool refreshed=astral_source_refresh_due(source_attempted,source!=nullptr,activated,now,source_retry_at);
        if(refreshed) {
            try { prepare_source(appearance,now); }
            catch(const std::exception& problem) {
                error=problem.what();source_attempted=true;
                source.reset();adapters.release();pose=WeakObject{};retained.release();
            }
        }
        if(!source || !pose.Get()) return;
        for(const auto& entry:entries) {
            if(!entry.observed.active() || !astral_kind_enabled(entry.observed.kind,options.faithful_css,options.stray_css) ||
               std::any_of(groups.begin(),groups.end(),[&](const auto& group){return group->observed.actor==entry.observed.actor && !group->parked;}) ||
               std::any_of(failed.begin(),failed.end(),[&](const auto& value){return value.actor==entry.observed.actor && value.activation==entry.activation;})) continue;
            // Source visibility and physics flags may change without a CSS
            // edit (for example, leaving Inventory). Capture them at activation.
            if(!refreshed) {
                refreshed=true;
                try { prepare_source(appearance,now); }
                catch(const std::exception& problem) {
                    error=problem.what();source.reset();adapters.release();
                    break;
                }
                if(!source) break;
            }
            auto cached=std::find_if(groups.begin(),groups.end(),[&](const auto& group) {
                return group->parked && group->observed.actor==entry.observed.actor;
            });
            if(cached!=groups.end()) {
                if((*cached)->can_resume(*source)) {
                    try { (*cached)->resume(entry,*source,adapters,now);++reused;continue; }
                    catch(const std::exception& problem) { error=problem.what(); }
                }
                if(!(*cached)->release()) throw std::runtime_error("Astral cached visual cleanup is incomplete");
                groups.erase(cached);++removed;
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
void AstralDoubles::update(void* engine,Appearance& appearance,bool enabled,const DoublesOptions& options,uint64_t now) {
    try { impl_->update(engine,appearance,enabled,options,now); }
    catch(const std::exception& problem) {
        impl_->error=problem.what();impl_->release_groups();impl_->next_poll=now+1000;
    }
}
bool AstralDoubles::clear() noexcept { return impl_->clear(); }
Json AstralDoubles::diagnostics() const {
    const auto parked=std::count_if(impl_->groups.begin(),impl_->groups.end(),[](const auto& group){return group->parked;});
    Json result={{"active",impl_->groups.size()-size_t(parked)},{"cached",parked},{"reused",impl_->reused},
        {"prepared",impl_->prepared},{"removed",impl_->removed},
        {"pose_rebinds",impl_->pose_rebinds},
        {"source_ready",impl_->source!=nullptr && impl_->pose.Get()!=nullptr},
        {"fallbacks",impl_->failed.size()},{"error",impl_->error}};
#ifdef CSS_INVENTORY_DEV
    result["creation_timing"]=astral_timing_report();
#endif
    return result;
}

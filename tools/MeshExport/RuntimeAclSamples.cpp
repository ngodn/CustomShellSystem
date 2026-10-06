#include "ACLDecompress.h"

// UE 5.6.1 ACLDecompressionImpl.h uses no rounding for linear playback.
// Nearest-key sampling skips interpolation across stripped ACL keys.
template<bool UseBind>
void Sample(const acl::compressed_tracks& tracks, FTransform* reference,
    FTrackToSkeletonMap* map, FTransform* output)
{
    const auto count = tracks.get_num_samples_per_track();
    DecompContextDefault context;
    context.initialize(tracks);
    FCUE4ParseOutputWriter<UseBind> writer(reference, map, output, count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const float time = rtm::scalar_min(float(i) / tracks.get_sample_rate(), tracks.get_finite_duration());
        context.seek(time, acl::sample_rounding_policy::none);
        writer.SampleIndex = i;
        context.decompress_tracks(writer);
    }
}

// The managed caller validates track headers, reference coverage and allocation size.
DLLEXPORT void nCSSReadACLData(const acl::compressed_tracks& tracks, FTransform* reference,
    FTrackToSkeletonMap* map, FTransform* output)
{
    if (tracks.get_default_scale() != 0) Sample<true>(tracks, reference, map, output);
    else Sample<false>(tracks, reference, map, output);
}

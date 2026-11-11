# Algorithm provenance and adaptations

- SMAA: Jorge Jimenez et al., upstream SMAA.hlsl and AreaTex/SearchTex headers (MIT). The OBS shader ports the spatial Low/Medium path: luma edge detection with local contrast adaptation, horizontal/vertical two-pixel searches, SearchTex correction, AreaTex lookup, neighborhood resolve. Alpha silhouette contrasts are added for OBS transparent sources. No temporal, diagonal or corner pass is claimed. LUT bytes are identical to the upstream downloaded headers, compiled once into the plugin.
- FXAA: independent implementation of the NVIDIA FXAA whitepaper pipeline: contrast early exit, 3x3 edge direction, eight-step endpoint search and optional subpixel lowpass. Uses premultiplied RGBA and chooses alpha only when silhouette contrast dominates RGB. This is spatial FXAA, not TAA or Native AA.
- RCAS: AMD FidelityFX FSR1 full-precision RCAS five-tap equations (MIT). Retains per-channel clipping limiters, strongest common negative lobe and optional high-pass noise reduction. Exact reciprocal guards avoid division by zero. Converts premultiplied input for color processing, uses center fallback for transparent neighbors and preserves center alpha. The exposed amount scales the lobe directly from 0 to 1 instead of the upstream stops-based UI conversion.

## CAS versus RCAS

Default CAS can also compile to five texture taps: unused diagonal loads disappear without CAS_BETTER_DIAGONALS. Therefore five taps alone does not establish that RCAS is faster. This release chooses RCAS for bounded one-pass operation, robust clipping control and optional noise protection with the same five samples. No upscaler, history buffers or CPU image-processing loop is introduced. Host timings are recorded; an integrated-GPU CAS-versus-RCAS benchmark is NOT RUN. Do not describe RCAS as universally the faster algorithm.

## Resource budget

At 1920x1080, SMAA uses one R8G8 edge target and two RGBA targets (weights and resolved output): about19.8 MiB per active layer, excluding the already-existing source capture and any later FX target. Shared immutable LUTs add about176 KiB per renderer. FXAA and RCAS use the existing RGBA ping surfaces. Dedicated resources/shaders are created on demand in graphics context and released when unused; steady frames reuse them. Controls default off. The stage's existing 2048-pixel source-capture cap remains in effect.

## Limits

These effects process the current layer texture before stage projection. They do not reconstruct missing source detail, use video motion vectors or fix aliases newly introduced by later stage perspective sampling. SMAA Low/Medium and FXAA have different filtering behavior; either can soften small text. Native AA is deferred in a separate engineering record.

## Primary-source archive

Downloaded originals and exact URLs, byte counts and SHA256 hashes: work/v3.4.1/references/sources.json. Upstream licenses are shipped in data/effects/AA_NOTICES.txt. The source release includes both immutable LUT headers and the adapted shader.

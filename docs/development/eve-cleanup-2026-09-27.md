# Eve workspace cleanup, September 27, 2026

User requested safe cleanup because storage was running low. Removed only eight superseded local Holiday Reveler Blender intermediates from `work/eve26`. All were untracked regular files with retained JSON receipts, and none was open according to lsof before cleanup. Source scripts and fitting history were reviewed first.

Removed 42,016,547,919 bytes (39.13 GiB logical size). After filesystem accounting settled, free space increased from about 30 GiB to 69 GiB. The Eve work directory fell from about 60 GiB to 20 GiB.

Retained F1 for initial comparison, F9 for topology/fitting, F10 for the weight baseline and export default, and F11 for the latest local hip repair. Original authoring/reference/Gemini sources, reports, renders, game files, releases and other agents' work were left untouched.

Historical references to deleted blends remain in the investigation log deliberately. To rerun those old stages, regenerate their intermediate files using the retained scripts and receipts. Continue current work from F10/F11; do not mistake a deleted candidate for a missing production source.

| Deleted file | Bytes | SHA-256 |
| --- | ---: | --- |
| `work/eve26/holiday-f2.blend` | 5253916152 | `80aa918b0690aa289933e7e912cc6a46ac5ec9df9978e4b25abf175cf3d7be4d` |
| `work/eve26/holiday-f3b.blend` | 5253873564 | `5be5c489183512a97984c90d765ba408d390f4abc926511107106d8d1e612a8d` |
| `work/eve26/holiday-f3d.blend` | 5253873429 | `2bcbd4c2fb0cd275f94d93587079ca645e31a0329d00a4cd5313a8398763e95b` |
| `work/eve26/holiday-f4.blend` | 5250982822 | `bb7059fdee8630f98f571780ae623e082c7db8fdd7306819ef52b324ca7ac629` |
| `work/eve26/holiday-f5.blend` | 5250982534 | `1d5a5c4d13e891df7c4f64eda2b2cc9db5970c0afa2bdd7fa2665bb54ba23f68` |
| `work/eve26/holiday-f6.blend` | 5250982543 | `1f7ea10c00ef0ec14fbcc884dd1820e7520ba9225b296ac8606f3b9b1ab000c6` |
| `work/eve26/holiday-f7.blend` | 5250975436 | `83b78d16716288f65d7c26e893a90394dd7f148d57a9bc8f6bf5f86ae8e514e5` |
| `work/eve26/holiday-f8.blend` | 5250961439 | `cd3e354ae5b25a3b3f4b427e42428b9574e2fb3158f1e9b0ac2fecda1fa832b7` |

Each corresponding `.json` receipt remains beside the deleted path. The local deletion journal is `work/eve26/cleanup-0927.json`. No broad cache, reference library or snapshot purge was performed.

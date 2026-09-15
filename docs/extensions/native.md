# Native C++ extension

The [native counter](../../examples/extensions/native-counter) is a complete C++23 example. It needs the bundled `native/include/cssx` headers and `native/vendor/nlohmann` headers, but no Unreal editor or UE4SS SDK. Only CSS's runtime bridge depends on UE4SS.

Export `cssx_get_extension` with C linkage. Return the `CssxExtension` table from [api.h](../../native/include/cssx/api.h), using ABI 1 and the table's actual size. [client.hpp](../../native/include/cssx/client.hpp) wraps JSON requests without passing STL ownership across DLL boundaries.

| Callback | Contract |
| --- | --- |
| `create(host)` | Return an instance or null on failure. Start passively. |
| `tick(instance, seconds)` | Short synchronous game-thread work; return 1 on success. |
| `model(instance, sink, output)` | Send UTF-8 JSON through the supplied sink. With a static menu, return value/state bindings. |
| `event(instance, json)` | Handle the selected control, return 1 on success. |
| `stop(instance)` | Restore owned changes. Return 0 if cleanup must retry. |
| `destroy(instance)` | Release your instance after successful stop. |

Catch exceptions inside every ABI callback. Sink data and input strings are borrowed; copy them before returning. Do not retain the sink or call engine services from a worker thread. A failure suspends the extension and starts cleanup; returning 0 from stop prevents unsafe unloading. Keep cleanup idempotent.

## Build on Windows

From an x64 Visual Studio developer terminal with CMake installed:

```bat
cmake -S examples/extensions/native-counter -B build/native-counter -A x64
cmake --build build/native-counter --config Release
copy build\native-counter\Release\counter.dll examples\extensions\native-counter\counter.dll
```

## Cross-compile on Linux

Use the repository's existing clang-cl/xwin toolchain, described in [native development](../native-development.md):

```sh
cmake -S examples/extensions/native-counter -B build/native-counter -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/native/toolchain-clang-cl.cmake"
cmake --build build/native-counter
cp build/native-counter/counter.dll examples/extensions/native-counter/counter.dll
```

Then [build the validator](getting-started.md#package) and package:

```sh
python3 tools/cssx_package.py examples/extensions/native-counter \
  --name 'CSSX_{name}_{author}_v{version}'
```

The DLL is ignored by Git and included only through the manifest's entry field. The resulting ZIP contains `examples.native-counter/`; install that folder under `CustomShellSystem/extensions/`. Change the namespace before publishing your own extension.

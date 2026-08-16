mkdir -p build/clay                                                       \
&& clang                                                                  \
-Wall                                                                     \
-Os                                                                       \
-DCLAY_WASM                                                               \
-mbulk-memory                                                             \
--target=wasm32                                                           \
-nostdlib                                                                 \
-Wl,--strip-all                                                           \
-Wl,--export-dynamic                                                      \
-Wl,--no-entry                                                            \
-Wl,--export=__heap_base                                                  \
-Wl,--export=ACTIVE_RENDERER_INDEX                                        \
-Wl,--export=THEME_INDEX                                                  \
-Wl,--initial-memory=6553600                                              \
-o build/clay/index.wasm                                                  \
main.c                                                                    \
&& cp index.html build/index.html                                         \
&& rm -rf build/clay/fonts build/clay/images                              \
&& cp -r fonts/ build/clay/fonts                                          \
&& cp -r images/ build/clay/images

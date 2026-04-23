@ECHO OFF

::QT程序打包
mkdir build-release

::快捷方式脚本
COPY shortcut.cmd  build-release\创建快捷方式.cmd


::目标程序
COPY build-relwithdebinfo\tl_assistant.exe                                              build-release

::配置相关
COPY c:\WORK\TlAssistant\build-relwithdebinfo\labelmerc.yaml                            build-release
COPY c:\WORK\TlAssistant\build-relwithdebinfo\app_config.json                           build-release
COPY c:\WORK\TlAssistant\build-relwithdebinfo\bpe_simple_vocab_16e6.txt.gz              build-release

::依赖库文件
COPY d:\3rd_party\opencv_4.13.0\x64\vc16\bin\opencv_world4130.dll                       build-release

COPY d:\3rd_party\ONNXRuntime-win-x64-gpu-1.25.0\lib\onnxruntime.dll                    build-release
COPY d:\3rd_party\ONNXRuntime-win-x64-gpu-1.25.0\lib\onnxruntime_providers_cuda.dll     build-release
COPY d:\3rd_party\ONNXRuntime-win-x64-gpu-1.25.0\lib\onnxruntime_providers_shared.dll   build-release



c:\Qt6\6.11.0\msvc2022_64\bin\windeployqt.exe --qmldir c:\Qt6\6.11.0\msvc2022_64\qml    build-release\tl_assistant.exe


::设置库隐藏
attrib +H build-release\Qt6*.dll
pause

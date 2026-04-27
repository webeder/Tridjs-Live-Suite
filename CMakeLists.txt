cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Bypass CMake 4.0 dropping support for CMake < 3.5" FORCE)
project(TridjsLiveSuite VERSION 1.0.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)

# Evitar alertas timestamp antigos do ONNX ou Github API
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
    juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip
)
FetchContent_MakeAvailable(juce)

# Fetch STK (Synthesis Toolkit in C++)
FetchContent_Declare(
    stk
    GIT_REPOSITORY https://github.com/thestk/stk.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stk)

# Fetch SoundTouch (BPM/Time Stretching)
FetchContent_Declare(
    soundtouch
    GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(soundtouch)

# Fetch ONNX Runtime (Pre-compiled headers and binaries para Windows)
set(ONNXRUNTIME_VERSION "1.16.3")
FetchContent_Declare(
    onnxruntime
    URL https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-win-x64-${ONNXRUNTIME_VERSION}.zip
)
FetchContent_MakeAvailable(onnxruntime)

# Adicionando o suporte oficial ao ASIO nativo do Windows
set(ASIO_SDK_PATH "C:/SDKs/ASIOSDK" CACHE PATH "Caminho master do ASIO SDK")

# Forçando o target interno do JUCE a enxergar as pastas RAW do SDK
target_include_directories(juce_audio_devices INTERFACE 
    "${ASIO_SDK_PATH}/common"
    "${ASIO_SDK_PATH}/host"
    "${ASIO_SDK_PATH}/host/pc"
)

# Create the standalone app executable
juce_add_gui_app(TridjsLiveSuite
    PRODUCT_NAME "Tridjs Live Suite"
    COMPANY_NAME "Tridjs"
    VERSION 1.0.0
    ICON_BIG "C:/TridjsMIDI/icone.png"
    ICON_SMALL "C:/TridjsMIDI/icone.png"
)
juce_generate_juce_header(TridjsLiveSuite)

# Sources
target_sources(TridjsLiveSuite
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MainComponent.h
        Source/TridjsLookAndFeel.cpp
        Source/TridjsLookAndFeel.h
        Source/HeaderComponent.cpp
        Source/HeaderComponent.h
        Source/StemsComponent.cpp
        Source/StemsComponent.h
        Source/PadComponent.cpp
        Source/PadComponent.h
        Source/PadsGridComponent.cpp
        Source/PadsGridComponent.h
        Source/FxRackComponent.cpp
        Source/FxRackComponent.h
        Source/SideBrowserComponent.cpp
        Source/SideBrowserComponent.h
        Source/FooterComponent.cpp
        Source/FooterComponent.h
        Source/AudioCore.cpp
        Source/AudioCore.h
        Source/SerialManager.cpp
        Source/SerialManager.h
        Source/InputManager.cpp
        Source/InputManager.h
        Source/RgbManager.cpp
        Source/RgbManager.h
)

# Adicionando Include Directories das dependencias extraídas 
target_include_directories(TridjsLiveSuite PRIVATE
    ${stk_SOURCE_DIR}/include
    ${soundtouch_SOURCE_DIR}/include
    ${onnxruntime_SOURCE_DIR}/include
)

# Compile options & modules
target_compile_definitions(TridjsLiveSuite
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_ASIO=1
)

if(MSVC)
    target_compile_options(TridjsLiveSuite PRIVATE /FS)
endif()


target_link_libraries(TridjsLiveSuite
    PRIVATE
        juce::juce_core
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_events
        juce::juce_data_structures
        
        # Link Custom Libs
        "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib"
        setupapi
)

# Copiar DLL do ONNX para o diretório do executável após a build
add_custom_command(TARGET TridjsLiveSuite POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll"
    $<TARGET_FILE_DIR:TridjsLiveSuite>
)

# Adicionar fontes do STK diretamente se necessário ou via target
target_compile_definitions(TridjsLiveSuite PRIVATE __WINDOWS_ASIO__ __WINDOWS_DS__)


include("D:/ScreenShare/screen-share/client-windows-zpn/screenshare_6.1/build/Desktop_Qt_6_11_1_MSVC2022_64bit-Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/qtproject_screenshare-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase;qtmultimedia")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "D:/ScreenShare/screen-share/client-windows-zpn/screenshare_6.1/build/Desktop_Qt_6_11_1_MSVC2022_64bit-Debug/qtproject_screenshare.exe"
    GENERATE_QT_CONF
)

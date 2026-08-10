#pragma once

#include <string>

class FileDialog {
public:
    // Opens native OS File Explorer open dialog.
    // Returns selected file path or empty string if user cancelled.
    static std::string OpenFile(const char* filter = "3D Models (*.gltf;*.glb;*.obj;*.stl)\0*.gltf;*.glb;*.obj;*.stl\0Wavefront OBJ (*.obj)\0*.obj\0STL Models (*.stl)\0*.stl\0glTF Models (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0");
};

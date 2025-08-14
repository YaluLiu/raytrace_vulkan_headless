#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "data_loader.h"
#include <iostream>

class ModelLoader {
public:
    // Pure virtual function to enforce implementation in derived classes
    virtual void loadModel(const std::string& filename) = 0;
    virtual ~ModelLoader() = default; // Virtual destructor for proper cleanup

    void print_info(){
        int print_num = 10;
        // 打印加载的顶点信息
        std::cout << "m_vertices size: " << m_vertices.size() << "\n";
        for (size_t i = 0; i < print_num; ++i) {
            const VertexObj& v = m_vertices[i];
            std::cout << "Vertex " << i << ": pos=(" << v.pos.x << ", " << v.pos.y << ", " << v.pos.z
                    << "), nrm=(" << v.nrm.x << ", " << v.nrm.y << ", " << v.nrm.z
                    << "), texCoord=(" << v.texCoord.x << ", " << v.texCoord.y
                    << "), color=(" << v.color.x << ", " << v.color.y << ", " << v.color.z << ")\n";
        }

        // 打印索引信息
        std::cout << "\n m_indices size: " << m_indices.size() << "\n";
        for (size_t i = 0; i < print_num; ++i) {
            if (i%3 == 0){
                std::cout << i/3 << " :";
            }
            std::cout <<  m_indices[i] << (i % 3 == 2 ? "\n" : ", ");
        }

        // 打印材质信息
        std::cout << "\n m_materials size: " << m_materials.size() << "\n";
        for (size_t i = 0; i < m_materials.size(); ++i) {
            const MaterialObj& m = m_materials[i];
            std::cout << "Material " << i << ":\n"
                    << "  Ambient: (" << m.ambient.x << ", " << m.ambient.y << ", " << m.ambient.z << ")\n"
                    << "  Diffuse: (" << m.diffuse.x << ", " << m.diffuse.y << ", " << m.diffuse.z << ")\n"
                    << "  Specular: (" << m.specular.x << ", " << m.specular.y << ", " << m.specular.z << ")\n"
                    << "  Shininess: " << m.shininess << "\n"
                    << "  Dissolve: " << m.dissolve << "\n"
                    << "  Illum: " << m.illum << "\n"
                    << "  TextureID: " << m.textureID << "\n";
        }

        // // 打印材质索引
        std::cout << "\n m_matIndx size: " << m_matIndx.size() << "\n";
        bool all_zero = false;
        for (size_t i = 0; i < print_num; ++i) {
            if(m_matIndx[i] != -1 && m_matIndx[i] != 0){
                all_zero = true;
                std::cout << "Material Indices has special value." << std::endl;
                break;
            }
        }
        if (all_zero == false){
            std::cout << "Material Indices is all 0.(-1 will be zero)" << std::endl;
        }

        // 打印材质索引
        std::cout << "\n m_textures size: " << m_textures.size() << "\n";
        for (size_t i = 0; i < m_textures.size(); ++i) {
            std::cout << m_textures[i] << std::endl;
        }
        
    }
    // Common member variables
    std::vector<VertexObj>   m_vertices;
    std::vector<uint32_t>    m_indices;
    std::vector<MaterialObj> m_materials;
    std::vector<std::string> m_textures;
    std::vector<int32_t>     m_matIndx;
};
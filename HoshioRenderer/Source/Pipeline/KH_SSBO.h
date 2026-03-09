#pragma once

#include "KH_Common.h"

template <typename T>
class KH_SSBO {
public:
    KH_SSBO(unsigned int bindPoint = 0) : BindPoint(bindPoint), ID(0), Size(0) {}
    KH_SSBO(KH_SSBO&& other) noexcept : ID(other.ID), BindPoint(other.BindPoint), Size(other.Size) {
        other.ID = 0;
        other.Size = 0;
    }

    ~KH_SSBO() {
        if (ID != 0) glDeleteBuffers(1, &ID);
    }
    KH_SSBO(const KH_SSBO&) = delete;
    KH_SSBO& operator=(const KH_SSBO&) = delete;

    void SetData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
        UpdateBuffer(data.data(), data.size(), usage);
    }

    void SetData(const T* data, size_t count, GLenum usage = GL_STATIC_DRAW) {
        UpdateBuffer(data, count, usage);
    }

    void GetData(std::vector<T>& outData) const {
        outData.resize(Size);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ID);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, Size * sizeof(T), outData.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void SetBindPoint(unsigned int BindPoint)
    {
        this->BindPoint = BindPoint;
    }

    void Bind(unsigned int BindPoint) 
    {
        SetBindPoint(BindPoint);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BindPoint, ID);
    }

    void Bind() const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BindPoint, ID);
    }

    void Unbind() const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BindPoint, 0);
    }

    unsigned int GetID() const { return ID; }
    size_t GetCount() const { return Size; }

private:
    unsigned int ID;
    unsigned int BindPoint;
    size_t Size; 

    void UpdateBuffer(const T* data, size_t count, GLenum usage) {
        if (ID == 0) {
            glGenBuffers(1, &ID);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ID);

        if (count != Size) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(T), data, usage);
            Size = count;
        }
        else {
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(T), data);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
};


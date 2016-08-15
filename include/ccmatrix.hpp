/*
 * Class CCMatrix: A 4x4 matrix that transforms 3D cartesian corrdinates into OpenGL camera space coordinates
 */

#ifndef CCMATRIX_H
#define CCMATRIX_H

#include "matrix.hpp"

template<class T>
class CCMatrix : public Matrix<T>
{
public:
    CCMatrix();
    ~CCMatrix();

    CCMatrix<T>& operator =(Matrix<T> other);

    CCMatrix<T>& operator =(CCMatrix<T> other);

    void setN(double p_n);
    void setF(double p_f);
    void setFov(double p_fov);
    void setWindow(size_t p_w, size_t p_h);
    void setProjection(bool value);

private:
    double p_n, p_f, p_fov;
    size_t p_w, p_h;
    bool   p_projection;
};

template<class T>
CCMatrix<T>::CCMatrix()
{
    this->set(4, 4, 0);
    p_w          = 1.0;
    p_projection = true;
    p_n          = 1.0;
    p_f          = 7.0;
    p_fov        = 10.0;
}


template<class T>
CCMatrix<T>::~CCMatrix()
{
    this->clear();
}


template<class T>
CCMatrix<T>& CCMatrix<T>::operator =(Matrix<T> other)
{
    this->swap(*this, other);

    return *this;
}


template<class T>
CCMatrix<T>& CCMatrix<T>::operator =(CCMatrix<T> other)
{
    this->swap(*this, other);

    return *this;
}


template<class T>
void CCMatrix<T>::setN(double N)
{
    p_n = N;
    setProjection(p_projection);
}


template<class T>
void CCMatrix<T>::setF(double F)
{
    p_f = F;
    setProjection(p_projection);
}


template<class T>
void CCMatrix<T>::setFov(double fov)
{
    p_fov = fov;

    if (fov <= 2.0)
    {
        p_projection = false;
    }
    else
    {
        p_projection = true;
    }

    setProjection(p_projection);
}


template<class T>
void CCMatrix<T>::setWindow(size_t w, size_t h)
{
    p_w = w;
    p_h = h;
    setProjection(p_projection);
}


template<class T>
void CCMatrix<T>::setProjection(bool value)
{
    p_projection = value;

    if (p_projection == true)
    {
        // Perspective
        this->p_buffer[0]  = 1.0 / std::tan(0.5 * p_fov * pi / 180.0) * static_cast<double> (p_h) / static_cast<double> (p_w);
        this->p_buffer[5]  = 1.0 / std::tan(0.5 * p_fov * pi / 180.0);
        this->p_buffer[10] = -(p_f + p_n) / (p_f - p_n);
        this->p_buffer[11] = -2.0 * p_f * p_n / (p_f - p_n);
        this->p_buffer[14] = -1.0;
        this->p_buffer[15] = 0.0;
    }
    else
    {
        // Orthonormal
        this->p_buffer[0]  = static_cast<double> (p_h) / static_cast<double> (p_w);
        this->p_buffer[5]  = 1.0;
        this->p_buffer[10] = -2.0 / (p_f - p_n);
        this->p_buffer[11] = -(p_f + p_n) / (p_f - p_n);
        this->p_buffer[14] = 0.0;
        this->p_buffer[15] = 1.0;
    }
}


#endif // CCMATRIX_H

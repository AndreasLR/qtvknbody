/*
 * Class RotationMatrix: A 4x4 or 3x3 rotation matrix
 */

#ifndef ROTATIONMATRIX_H
#define ROTATIONMATRIX_H

#include "matrix.hpp"

template<class T>
class RotationMatrix : public Matrix<T>
{
public:
    RotationMatrix();
    ~RotationMatrix();

    RotationMatrix<T>& operator =(Matrix<T> other);

    RotationMatrix<T>& operator =(RotationMatrix<T> other);

    void setXRotation(double value);
    void setYRotation(double value);
    void setZRotation(double value);
    void setArbRotation(double zeta, double eta, double gamma);

    RotationMatrix<T> to3x3();

    RotationMatrix<T> getXRotation(double value);

    RotationMatrix<T> getYRotation(double value);

    RotationMatrix<T> getZRotation(double value);

    RotationMatrix<T> getArbRotation(double zeta, double eta, double gamma);

    void setFrom3x3(Matrix<T> mat);
};

template<class T>
RotationMatrix<T>::RotationMatrix()
{
    this->setIdentity(4);
}


template<class T>
RotationMatrix<T> RotationMatrix<T>::to3x3()
{
    RotationMatrix<T> tmp;

    tmp.setIdentity(3);

    tmp[0] = this->at(0);
    tmp[1] = this->at(1);
    tmp[2] = this->at(2);
    tmp[3] = this->at(4);
    tmp[4] = this->at(5);
    tmp[5] = this->at(6);
    tmp[6] = this->at(8);
    tmp[7] = this->at(9);
    tmp[8] = this->at(10);

    return tmp;
}


template<class T>
RotationMatrix<T>::~RotationMatrix()
{
    this->clear();
}


template<class T>
RotationMatrix<T>& RotationMatrix<T>::operator =(Matrix<T> other)
{
    this->swap(other);
    return *this;
}


template<class T>
RotationMatrix<T>& RotationMatrix<T>::operator =(RotationMatrix<T> other)
{
    this->swap(other);
    return *this;
}


template<class T>
void RotationMatrix<T>::setFrom3x3(Matrix<T> mat)
{
    this->setIdentity(4);

    this->data()[0]  = mat[0];
    this->data()[1]  = mat[1];
    this->data()[2]  = mat[2];
    this->data()[4]  = mat[3];
    this->data()[5]  = mat[4];
    this->data()[6]  = mat[5];
    this->data()[8]  = mat[6];
    this->data()[9]  = mat[7];
    this->data()[10] = mat[8];
}


template<class T>
void RotationMatrix<T>::setXRotation(double value)
{
    this->setIdentity(4);

    double s = std::sin(value);
    double c = std::cos(value);

    this->data()[5]  = c;
    this->data()[6]  = s;
    this->data()[9]  = -s;
    this->data()[10] = c;
}


template<class T>
void RotationMatrix<T>::setYRotation(double value)
{
    this->setIdentity(4);

    double s = std::sin(value);
    double c = std::cos(value);

    this->data()[0]  = c;
    this->data()[2]  = -s;
    this->data()[8]  = s;
    this->data()[10] = c;
}


template<class T>
void RotationMatrix<T>::setZRotation(double value)
{
    this->setIdentity(4);

    double s = std::sin(value);
    double c = std::cos(value);

    this->data()[0] = c;
    this->data()[1] = s;
    this->data()[4] = -s;
    this->data()[5] = c;
}


template<class T>
void RotationMatrix<T>::setArbRotation(double zeta, double eta, double gamma)
{
    // Rotation around a axis whose tilts are given by zeta and eta
    RotationMatrix<T> RyPlus, RxPlus, RzGamma, RxMinus, RyMinus;

    RyPlus.setYRotation(zeta);
    RxPlus.setXRotation(eta);
    RzGamma.setZRotation(gamma);
    RxMinus.setXRotation(-eta);
    RyMinus.setYRotation(-zeta);

    (*this) = RyPlus * RxPlus * RzGamma * RxMinus * RyMinus;
}


template<class T>
RotationMatrix<T> RotationMatrix<T>::getXRotation(double value)
{
    RotationMatrix m;

    double s = std::sin(value);
    double c = std::cos(value);

    m[5]  = c;
    m[6]  = s;
    m[9]  = -s;
    m[10] = c;

    return m;
}


template<class T>
RotationMatrix<T> RotationMatrix<T>::getYRotation(double value)
{
    RotationMatrix m;

    double s = std::sin(value);
    double c = std::cos(value);

    m[0]  = c;
    m[2]  = -s;
    m[8]  = s;
    m[10] = c;

    return m;
}


template<class T>
RotationMatrix<T> RotationMatrix<T>::getZRotation(double value)
{
    RotationMatrix m;

    double s = std::sin(value);
    double c = std::cos(value);

    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;

    return m;
}


template<class T>
RotationMatrix<T> RotationMatrix<T>::getArbRotation(double zeta, double eta, double gamma)
{
    // Rotation around a axis whose tilts are given by zeta and eta
    RotationMatrix<T> RyPlus, RxPlus, RzGamma, RxMinus, RyMinus;

    RyPlus.setYRotation(zeta);
    RxPlus.setXRotation(eta);
    RzGamma.setZRotation(gamma);
    RxMinus.setXRotation(-eta);
    RyMinus.setYRotation(-zeta);

    return RyPlus * RxPlus * RzGamma * RxMinus * RyMinus;
}


#endif // ROTATIONMATRIX_H

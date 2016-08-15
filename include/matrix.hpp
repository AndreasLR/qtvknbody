/*
 * Class Matrix: A container class that represents an NxM matrix that has various functionality like multiplication etc. Very convenient for OpenGl matrix calculations and the like
 */

#ifndef MATRIX_H
#define MATRIX_H

/*
 * A class supplying matrix functionality
 * */

//#include <cassert>
#include <sstream>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <cstring>
#include <vector>

#include <QVector>
#include <QDebug>
#include <QDataStream>

const double pi = 4.0 * atan(1.0);

template<class T>
class Matrix
{
public:
    Matrix();
    Matrix(size_t p_m, size_t p_n, T value);
    Matrix(size_t p_m, size_t p_n);
    Matrix(const Matrix& other);
    Matrix(Matrix&& other);
    Matrix<T>& operator =(Matrix<T> other);

    ~Matrix();

    // Operators
    Matrix operator *(const Matrix&) const;
    Matrix operator *(const T&) const;
    Matrix operator /(const Matrix&) const;
    Matrix operator /(const T&) const;
    Matrix operator +=(const Matrix&);
    Matrix operator +=(const T&) const;
    Matrix operator -=(const Matrix&) const;
    Matrix operator -=(const T&) const;

    Matrix operator -(const Matrix&) const;
    Matrix operator -(const T&) const;
    Matrix operator -() const;
    Matrix operator +(const Matrix&) const;
    Matrix operator +(const T&) const;
    Matrix operator +() const;

    T&       operator[](const size_t index);
    const T& operator[](const size_t index) const;

    // Utility
    Matrix<T> inverse(int verbose = false) const;

    Matrix<T> inverse4x4(int verbose = false) const;

    Matrix<T> colmajor() const;

    Matrix<float> toFloat() const;

    Matrix<double> toDouble() const;

    Matrix<int> toInt() const;

    QVector<T> toQVector() const;

    //        T max() const;
    //        T min() const;
    void minmax(T *min, T *max) const;
    T sum() const;
    T sum(size_t first, size_t last) const;

    void setIdentity(size_t p_n);
    void set(size_t p_m, size_t p_n);
    void set(size_t p_m, size_t p_n, T value);
    void setDeep(size_t p_m, size_t p_n, T *p_buffer);
    void reserve(size_t p_m, size_t p_n);
    void resize(size_t p_m, size_t p_n);
    void clear();

    const T *data() const;
    T *data();
    T at(size_t index);

    size_t m() const;
    size_t n() const;
    size_t size() const;
    size_t bytes() const;

    void print(int precision = 0, const char *id = "") const;

protected:
    size_t         p_m;
    size_t         p_n;
    std::vector<T> p_buffer;

    void swap(Matrix& other);
};

template<class T>
QDebug operator<<(QDebug dbg, const Matrix<T>& m)
{
    dbg.nospace() << "(...)";
    return dbg.maybeSpace();
}


template<class T>
QDataStream& operator<<(QDataStream& out, const Matrix<T>& mat)
{
    out << static_cast<qint64> (mat.m()) << static_cast<qint64> (mat.n()) << mat.toQVector();

    return out;
}


template<class T>
QDataStream& operator>>(QDataStream& in, Matrix<T>& mat)
{
    qint64 m;
    qint64 n;

    QVector<T> data;

    in >> m >> n >> data;

    mat.setDeep(static_cast<size_t> (m), static_cast<size_t> (n), data.data());
    return in;
}


template<class T>
Matrix<T> operator*(const T& factor, const Matrix<T>& M)
{
    return M * factor;
}


template<class T>
Matrix<T> operator-(const T& value, const Matrix<T>& M)
{
    return -M + value;
}


template<class T>
Matrix<T> operator+(const T& value, const Matrix<T>& M)
{
    return M + value;
}


template<class T>
Matrix<T>::Matrix(size_t m, size_t n, T value)
{
    this->p_m = 0;
    this->p_n = 0;
    this->set(m, n, value);
}


template<class T>
Matrix<T>::Matrix(size_t m, size_t n)
{
    this->p_m = 0;
    this->p_n = 0;
    this->reserve(m, n);
}


template<class T>
Matrix<T>::Matrix()
{
    this->p_m = 0;
    this->p_n = 0;
}


template<class F>
Matrix<F> vecCross(const Matrix<F> A, const Matrix<F> B)
{
    Matrix<F> C = A;

    if ((A.m() * A.n() != 3) || (B.m() * B.n() != 3))
    {
        qWarning() << "Attempt to take cross product of vectors with dimensions" << A.m() << "x" << A.n() << "and" << B.m() << "x" << B.n();
    }
    else
    {
        C[0] = A[1] * B[2] - A[2] * B[1];
        C[1] = A[2] * B[0] - A[0] * B[2];
        C[2] = A[0] * B[1] - A[1] * B[0];
    }

    return C;
}


template<class F>
F vecDot(const Matrix<F> A, const Matrix<F> B)
{
    F value = 0;

    for (size_t i = 0; i < A.size(); i++)
    {
        value += A[i] * B[i];
    }

    return value;
}


template<class F>
F vecLength(const Matrix<F> A)
{
    F sum = 0;

    for (size_t i = 0; i < A.m() * A.n(); i++)
    {
        sum += A[i] * A[i];
    }

    return sqrt(sum);
}


template<class F>
F zeta(const Matrix<F> A)
{
    F x = A[0], z = A[2];

    return atan2(-x, z);
}


template<class F>
F eta(const Matrix<F> A)
{
    F x = A[0], y = A[1], z = A[2];

    return asin(y / sqrt(x * x + y * y + z * z));
}


template<class F>
Matrix<F> vecNormalize(const Matrix<F> A)
{
    return A * (1.0 / vecLength(A));
}


template<class F>
std::ostream& operator <<(std::ostream& stream, const Matrix<F> M)
{
    std::stringstream ss;
    ss << std::endl;

    ss << "(" << M.m() << ", " << M.n() << "):" << std::endl;

    for (size_t i = 0; i < M.m(); i++)
    {
        if (i == 0)
        {
            ss << "{{ ";
        }
        else
        {
            ss << " { ";
        }

        for (size_t j = 0; j < M.n(); j++)
        {
            ss << std::setprecision(2) << std::fixed << M.data()[i * M.n() + j];

            if (j != M.n() - 1)
            {
                ss << ", ";
            }
        }

        if (i == M.m() - 1)
        {
            ss << " }}" << std::endl;
        }
        else
        {
            ss << " }," << std::endl;
        }
    }

    stream << ss.str();

    return stream;
}


template<class T>
QVector<T> Matrix<T>::toQVector() const
{
    QVector<T> buf;
    buf.resize(this->size());

    for (size_t i = 0; i < this->size(); i++)
    {
        buf[i] = this->p_buffer[i];
    }

    return buf;
}


template<class T>
Matrix<T>::Matrix(const Matrix& other) :
    p_m(other.p_m),
    p_n(other.p_n),
    p_buffer(other.p_buffer)
{
}


template<class T>
Matrix<T>::Matrix(Matrix&& other) :
    p_m(std::move(other.p_m)),
    p_n(std::move(other.p_n)),
    p_buffer(std::move(other.p_buffer))
{
}


template<class T>
Matrix<T>::~Matrix()
{
    if (p_m * p_n > 0)
    {
        p_m = 0;
        p_n = 0;
    }
}


template<class T>
void Matrix<T>::resize(size_t m, size_t n)
{
    // This resize function retains any old values and fills voids with zeros
    Matrix<T> temp(m, n);

    for (size_t i = 0; i < m; i++)
    {
        for (size_t j = 0; j < n; j++)
        {
            if ((i < this->p_m) && (j < this->p_n))
            {
                temp[n * i + j] = p_buffer[n * i + j];
            }
            else
            {
                temp[n * i + j] = 0;
            }
        }
    }

    *this = temp;
}


template<class T>
T Matrix<T>::sum() const
{
    T sum = 0;

    for (size_t i = 0; i < p_n; i++)
    {
        for (size_t j = 0; j < p_m; j++)
        {
            sum += p_buffer[i * p_m + j];
        }
    }

    return sum;
}


template<class T>
T Matrix<T>::sum(size_t first, size_t last) const
{
    T sum = 0;

    for (size_t i = first; i < (std::min)(last + 1, p_m * p_n); i++)
    {
        sum += p_buffer[i];
    }

    return sum;
}


template<class T>
void Matrix<T>::minmax(T *min, T *max) const
{
    *min = p_buffer[0];
    *max = p_buffer[0];

    for (size_t i = 0; i < p_n; i++)
    {
        for (size_t j = 0; j < p_m; j++)
        {
            T value = p_buffer[i * p_m + j];

            if (value > *max)
            {
                *max = value;
            }
            else if (value < *min)
            {
                *min = value;
            }
        }
    }
}


template<class T>
void Matrix<T>::swap(Matrix& other)
{
    std::swap(this->p_m, other.p_m);
    std::swap(this->p_n, other.p_n);
    std::swap(this->p_buffer, other.p_buffer);
}


template<class T>
Matrix<float> Matrix<T>::toFloat() const
{
    Matrix<float> buf(this->p_m, this->p_n);

    for (size_t i = 0; i < this->p_m * this->p_n; i++)
    {
        buf[i] = static_cast<float> (this->p_buffer[i]);
    }

    return buf;
}


template<class T>
Matrix<double> Matrix<T>::toDouble() const
{
    Matrix<double> buf(this->p_m, this->p_n);

    for (size_t i = 0; i < this->p_m * this->p_n; i++)
    {
        buf[i] = static_cast<double> (this->p_buffer[i]);
    }

    return buf;
}


template<class T>
Matrix<int> Matrix<T>::toInt() const
{
    Matrix<int> buf(this->p_m, this->p_n);

    for (size_t i = 0; i < this->p_m * this->p_n; i++)
    {
        buf[i] = static_cast<int> (this->p_buffer[i]);
    }

    return buf;
}


template<class T>
void Matrix<T>::setIdentity(size_t n)
{
    this->set(n, n, 0);

    for (size_t i = 0, j = 0; i < n; i++, j++)
    {
        p_buffer[i * n + j] = 1;
    }
}


template<class T>
size_t Matrix<T>::bytes() const
{
    return p_m * p_n * sizeof(T);
}


template<class T>
Matrix<T> Matrix<T>::colmajor() const
{
    Matrix<T> ColMajor;
    ColMajor.reserve(this->n(), this->m());
    size_t count = 0;

    for (size_t i = 0; i < this->m(); i++)
    {
        for (size_t j = 0; j < this->n(); j++)
        {
            ColMajor[i * this->n() + j] = this->data()[(count % this->m()) * this->n() + (count / this->m()) % this->n()];
            count++;
        }
    }

    return ColMajor;
}


template<class T>
Matrix<T> Matrix<T>::inverse4x4(int verbose) const
{
    Q_UNUSED(verbose);

    if ((p_m != 4) || (p_n != 4))
    {
        qWarning() << "Matrix is can not be inverted: m (= " << p_m << ") != n (=" << p_n << ")";
    }

    Matrix<T> INV(4, 4);
    Matrix<T> M = this->colmajor();

    T det;
    INV[0] = M[5] * M[10] * M[15] -
             M[5] * M[11] * M[14] -
             M[9] * M[6] * M[15] +
             M[9] * M[7] * M[14] +
             M[13] * M[6] * M[11] -
             M[13] * M[7] * M[10];

    INV[4] = -M[4] * M[10] * M[15] +
             M[4] * M[11] * M[14] +
             M[8] * M[6] * M[15] -
             M[8] * M[7] * M[14] -
             M[12] * M[6] * M[11] +
             M[12] * M[7] * M[10];

    INV[8] = M[4] * M[9] * M[15] -
             M[4] * M[11] * M[13] -
             M[8] * M[5] * M[15] +
             M[8] * M[7] * M[13] +
             M[12] * M[5] * M[11] -
             M[12] * M[7] * M[9];

    INV[12] = -M[4] * M[9] * M[14] +
              M[4] * M[10] * M[13] +
              M[8] * M[5] * M[14] -
              M[8] * M[6] * M[13] -
              M[12] * M[5] * M[10] +
              M[12] * M[6] * M[9];

    INV[1] = -M[1] * M[10] * M[15] +
             M[1] * M[11] * M[14] +
             M[9] * M[2] * M[15] -
             M[9] * M[3] * M[14] -
             M[13] * M[2] * M[11] +
             M[13] * M[3] * M[10];

    INV[5] = M[0] * M[10] * M[15] -
             M[0] * M[11] * M[14] -
             M[8] * M[2] * M[15] +
             M[8] * M[3] * M[14] +
             M[12] * M[2] * M[11] -
             M[12] * M[3] * M[10];

    INV[9] = -M[0] * M[9] * M[15] +
             M[0] * M[11] * M[13] +
             M[8] * M[1] * M[15] -
             M[8] * M[3] * M[13] -
             M[12] * M[1] * M[11] +
             M[12] * M[3] * M[9];

    INV[13] = M[0] * M[9] * M[14] -
              M[0] * M[10] * M[13] -
              M[8] * M[1] * M[14] +
              M[8] * M[2] * M[13] +
              M[12] * M[1] * M[10] -
              M[12] * M[2] * M[9];

    INV[2] = M[1] * M[6] * M[15] -
             M[1] * M[7] * M[14] -
             M[5] * M[2] * M[15] +
             M[5] * M[3] * M[14] +
             M[13] * M[2] * M[7] -
             M[13] * M[3] * M[6];

    INV[6] = -M[0] * M[6] * M[15] +
             M[0] * M[7] * M[14] +
             M[4] * M[2] * M[15] -
             M[4] * M[3] * M[14] -
             M[12] * M[2] * M[7] +
             M[12] * M[3] * M[6];

    INV[10] = M[0] * M[5] * M[15] -
              M[0] * M[7] * M[13] -
              M[4] * M[1] * M[15] +
              M[4] * M[3] * M[13] +
              M[12] * M[1] * M[7] -
              M[12] * M[3] * M[5];

    INV[14] = -M[0] * M[5] * M[14] +
              M[0] * M[6] * M[13] +
              M[4] * M[1] * M[14] -
              M[4] * M[2] * M[13] -
              M[12] * M[1] * M[6] +
              M[12] * M[2] * M[5];

    INV[3] = -M[1] * M[6] * M[11] +
             M[1] * M[7] * M[10] +
             M[5] * M[2] * M[11] -
             M[5] * M[3] * M[10] -
             M[9] * M[2] * M[7] +
             M[9] * M[3] * M[6];

    INV[7] = M[0] * M[6] * M[11] -
             M[0] * M[7] * M[10] -
             M[4] * M[2] * M[11] +
             M[4] * M[3] * M[10] +
             M[8] * M[2] * M[7] -
             M[8] * M[3] * M[6];

    INV[11] = -M[0] * M[5] * M[11] +
              M[0] * M[7] * M[9] +
              M[4] * M[1] * M[11] -
              M[4] * M[3] * M[9] -
              M[8] * M[1] * M[7] +
              M[8] * M[3] * M[5];

    INV[15] = M[0] * M[5] * M[10] -
              M[0] * M[6] * M[9] -
              M[4] * M[1] * M[10] +
              M[4] * M[2] * M[9] +
              M[8] * M[1] * M[6] -
              M[8] * M[2] * M[5];

    det = M[0] * INV[0] + M[1] * INV[4] + M[2] * INV[8] + M[3] * INV[12];

    if (det == 0)
    {
        qWarning("Determinant is zero");
    }

    det = 1.0 / det;

    INV = INV * det;

    return INV.colmajor();
}


template<class T>
Matrix<T> Matrix<T>::inverse(int verbose) const
{
    if (p_m != p_n)
    {
        qDebug() << "Matrix is can not be inverted: m (= " << p_m << ") != n (=" << p_n << ")";
    }

    Matrix<T> L, y, I, U, x, rescue;
    L.set(p_n, p_n, 0);
    y.set(p_n, p_n, 0);
    I.setIdentity(p_n);

    U.set(p_n, p_n, 0);
    x.set(p_n, p_n, 0);
    rescue.setIdentity(p_n);

    /* Ax = LUx = I method */

    /* LU Decomposition */
    int i, j, k;
    T   sum;

    if (verbose)
    {
        this->print(3, "M");
    }

    for (i = 0; i < static_cast<int> (p_n); i++)
    {
        U[i * p_n + i] = 1;
    }

    for (j = 0; j < static_cast<int> (p_n); j++)
    {
        for (i = j; i < static_cast<int> (p_n); i++)
        {
            sum = 0;

            for (k = 0; k < j; k++)
            {
                sum += L[i * p_n + k] * U[k * p_n + j];
            }

            L[i * p_n + j] = p_buffer[i * p_n + j] - sum;
        }

        for (i = j; i < static_cast<int> (p_n); i++)
        {
            sum = 0;

            for (k = 0; k < j; k++)
            {
                sum += L[j * p_n + k] * U[k * p_n + i];
            }

            if (L[j * p_n + j] == 0)
            {
                qWarning("Determinant close to zero.");
                return rescue;
            }

            U[j * p_n + i] = (p_buffer[j * p_n + i] - sum) / L[j * p_n + j];
        }
    }

    if (verbose)
    {
        L.print(3, "L");
        U.print(3, "U");

        (L * U).print(3, "L*U");
    }

    /* Solve LY = I for Y (= UX) */
    for (i = 0; i < static_cast<int> (p_n); i++)
    {
        for (j = 0; j < static_cast<int> (p_n); j++)
        {
            T zum = 0;

            for (k = 0; k < static_cast<int> (p_n); k++)
            {
                if (k != i)
                {
                    zum += y[k * p_n + j] * L[i * p_n + k];
                }
            }

            y[i * p_n + j] = (I[i * p_n + j] - zum) / L[i * p_n + i];
        }
    }

    if (verbose)
    {
        y.print(3, "Y");

        (L * y).print(3, "LY = I");
    }

    /* Solve UX = Y for X */
    for (i = p_n - 1; i >= 0; --i)
    {
        for (j = p_n - 1; j >= 0; --j)
        {
            T zum = 0;

            for (k = 0; k < static_cast<int> (p_n); k++)
            {
                if (k != i)
                {
                    zum += x[k * p_n + j] * U[i * p_n + k];
                }
            }

            x[i * p_n + j] = (y[i * p_n + j] - zum) / U[i * p_n + i];
        }
    }

    if (verbose)
    {
        x.print(3, "X");

        (U * x).print(3, "UX = Y");

        (*this * x).print(3, "I");
    }

    return x;
}


template<class T>
Matrix<T> Matrix<T>::operator +(const Matrix& M) const
{
    Matrix<T> c(*this);

    if ((this->p_n != M.n()) || (this->p_m != M.m()))
    {
        qFatal("Matrix dimesions do not agree!");
        return c;
    }

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] += M[i * c.n() + j];
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator +(const T& value) const
{
    Matrix<T> c(*this);

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] += value;
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator -(const Matrix& M) const
{
    Matrix<T> c(*this);

    if ((this->p_n != M.n()) || (this->p_m != M.m()))
    {
        qFatal("Matrix dimesions do not agree!");
        return c;
    }

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] -= M[i * c.n() + j];
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator -(const T& value) const
{
    Matrix<T> c(*this);

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] -= value;
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator -() const
{
    Matrix<T> c(*this);

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] = -c[i * c.n() + j];
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator +() const
{
    Matrix<T> c(*this);

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] = +c[i * c.n() + j];
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator *(const T& factor) const
{
    Matrix<T> c(*this);

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] = c[i * c.n() + j] * factor;
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator *(const Matrix& M) const
{
    Matrix<T> c;

    if (this->p_n != M.m())
    {
        qWarning(QString("Matrix dimesions do not agree! (" + QString::number(p_m) + ", " + QString::number(p_n) + ") x (" + QString::number(M.m()) + ", " + QString::number(M.n()) + ")").toStdString().c_str());
        return c;
    }

    c.set(this->p_m, M.n(), 0.0f);

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            for (size_t k = 0; k < this->p_n; k++)
            {
                c[i * c.n() + j] += this->p_buffer[i * this->p_n + k] * M[k * c.n() + j];
            }
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator /(const T& value) const
{
    Matrix<T> c(*this);

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] = c[i * c.n() + j] / value;
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator /(const Matrix& M) const
{
    Matrix<T> c(*this);

    if ((this->p_n != M.n()) || (this->p_m != M.m()))
    {
        qFatal("Matrix dimesions do not agree!");
        return c;
    }

    for (size_t i = 0; i < c.m(); i++)
    {
        for (size_t j = 0; j < c.n(); j++)
        {
            c[i * c.n() + j] /= M[i * c.n() + j];
        }
    }

    return c;
}


template<class T>
Matrix<T> Matrix<T>::operator +=(const T& value) const
{
    for (size_t i = 0; i < p_m; i++)
    {
        for (size_t j = 0; j < p_n; j++)
        {
            p_buffer[i * p_n + j] += value;
        }
    }

    return *this;
}


template<class T>
Matrix<T> Matrix<T>::operator +=(const Matrix& M)
{
    if ((this->p_n != M.n()) || (this->p_m != M.m()))
    {
        qFatal("Matrix dimesions do not agree!");
        return *this;
    }

    for (size_t i = 0; i < p_m; i++)
    {
        for (size_t j = 0; j < p_n; j++)
        {
            p_buffer[i * p_n + j] += M[i * M.n() + j];
        }
    }

    return *this;
}


template<class T>
Matrix<T> Matrix<T>::operator -=(const T& value) const
{
    for (size_t i = 0; i < p_m; i++)
    {
        for (size_t j = 0; j < p_n; j++)
        {
            p_buffer[i * p_n + j] -= value;
        }
    }

    return *this;
}


template<class T>
Matrix<T> Matrix<T>::operator -=(const Matrix& M) const
{
    if ((this->p_n != M.n()) || (this->p_m != M.m()))
    {
        qFatal("Matrix dimesions do not agree!");
        return *this;
    }

    for (size_t i = 0; i < p_m; i++)
    {
        for (size_t j = 0; j < p_n; j++)
        {
            p_buffer[i * p_n + j] -= M[i * M.n() + j];
        }
    }

    return *this;
}


template<class T>
void Matrix<T>::print(int precision, const char *id) const
{
    std::stringstream ss;
    ss << std::endl;

    if (strlen(id) > 0)
    {
        ss << id << "(" << this->p_m << ", " << this->p_n << "):" << std::endl;
    }

    for (size_t i = 0; i < p_m; i++)
    {
        if (i == 0)
        {
            ss << " [ ";
        }
        else
        {
            ss << " [ ";
        }

        for (size_t j = 0; j < p_n; j++)
        {
            ss << std::setprecision(precision) << std::fixed << this->p_buffer[i * p_n + j];

            if (j != p_n - 1)
            {
                ss << ", ";
            }
        }

        if (i == p_m - 1)
        {
            ss << " ]" << std::endl;
        }
        else
        {
            ss << " ]" << std::endl;
        }
    }

    qDebug() << ss.str().c_str();
}


template<class T>
Matrix<T>& Matrix<T>::operator =(Matrix<T> other)
{
    swap(other);
    return *this;
}


template<class T>
void Matrix<T>::set(size_t m, size_t n)
{
    this->clear();
    this->p_m = m;
    this->p_n = n;
    this->p_buffer.resize(m * n);
}


template<class T>
void Matrix<T>::set(size_t m, size_t n, T value)
{
    this->clear();
    this->p_m = m;
    this->p_n = n;
    this->p_buffer.resize(m * n);

    for (size_t i = 0; i < m * n; i++)
    {
        this->p_buffer[i] = value;
    }
}


template<class T>
void Matrix<T>::setDeep(size_t m, size_t n, T *buffer)
{
    this->clear();
    this->p_m = m;
    this->p_n = n;
    this->p_buffer.resize(m * n);

    for (size_t i = 0; i < m * n; i++)
    {
        this->p_buffer[i] = buffer[i];
    }
}


template<class T>
void Matrix<T>::reserve(size_t m, size_t n)
{
    this->clear();
    this->p_m = m;
    this->p_n = n;
    this->p_buffer.resize(m * n);
}


template<class T>
T& Matrix<T>::operator[](const size_t index)
{
    if (index >= p_m * p_n)
    {
        qDebug() << index << ">=" << p_m << "x" << p_n;
    }

    Q_ASSERT(index < p_m * p_n);

    return p_buffer[index];
}


template<class T>
const T& Matrix<T>::operator[](const size_t index) const
{
    if (index >= p_m * p_n)
    {
        qDebug() << index << ">=" << p_m << "x" << p_n;
    }

    Q_ASSERT(index < p_m * p_n);

    return p_buffer[index];
}


template<class T>
void Matrix<T>::clear()
{
    if (p_m * p_n > 0)
    {
        // Since C++ vectors are optimized for speed, calling clear will not always free up the associated memory. It will be freed if the destructor is called, though. Here we swap the vector data over to a decoy which is destroyed when the function returns.
        std::vector<T> decoy;
        std::vector<T> (decoy).swap(p_buffer);
        // To make the function independet of type (T), we can just call std::vector.clear() without the preceeding lines
        this->p_buffer.clear();
        this->p_m = 0;
        this->p_n = 0;
    }
}


template<class T>
T *Matrix<T>::data()
{
    return this->p_buffer.data();
}


template<class T>
const T *Matrix<T>::data() const
{
    return this->p_buffer.data();
}


template<class T>
T Matrix<T>::at(size_t index)
{
    Q_ASSERT(index < p_m * p_n);

    return p_buffer[index];
}


template<class T>
size_t Matrix<T>::m() const
{
    return this->p_m;
}


template<class T>
size_t Matrix<T>::n() const
{
    return this->p_n;
}


template<class T>
size_t Matrix<T>::size() const
{
    return p_m * p_n;
}


#endif

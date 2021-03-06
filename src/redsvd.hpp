/*
 * Fix bug and implement slightlydifferent version of randomized SVD
 *  as same way as swell.
 * See Algorithm 5 in [Dhillon+ 2015].
 * cf. swell - GitHub : https://github.com/paramveerdhillon/swell
 *
 * Copyright (c) 2016 Takamasa Oshikiri
 *
 * 
 * based on redsvd-h
 * 
 * Copyright (c) 2014 Nicolas Tessore
 *
 * 
 * based on RedSVD
 * 
 * Copyright (c) 2010 Daisuke Okanohara
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above Copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above Copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the authors nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 */

#ifndef REDSVD_MODULE_H
#define REDSVD_MODULE_H

#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include <cstdlib>
#include <cmath>

namespace RedSVD
{
  template<typename Scalar>
  inline void sample_gaussian(Scalar& x, Scalar& y)
  {
    using std::sqrt;
    using std::log;
    using std::cos;
    using std::sin;
		
    const Scalar _PI(3.1415926535897932384626433832795028841971693993751);
		
    Scalar v1 = (Scalar)(std::rand() + Scalar(1)) / ((Scalar)RAND_MAX+Scalar(2));
    Scalar v2 = (Scalar)(std::rand() + Scalar(1)) / ((Scalar)RAND_MAX+Scalar(2));
    Scalar len = sqrt(Scalar(-2) * log(v1));
    x = len * cos(Scalar(2) * _PI * v2);
    y = len * sin(Scalar(2) * _PI * v2);
  }
	
  template<typename MatrixType>
  inline void sample_gaussian(MatrixType& mat)
  {
    typedef typename MatrixType::Index Index;
		
    for(Index i = 0; i < mat.rows(); ++i)
      {
        for(Index j = 0; j+1 < mat.cols(); j += 2)
          sample_gaussian(mat(i, j), mat(i, j+1));
        if(mat.cols() % 2)
          sample_gaussian(mat(i, mat.cols()-1), mat(i, mat.cols()-1));
      }
  }
	
  template<typename MatrixType>
  inline void gram_schmidt(MatrixType& mat)
  {
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
		
    static const Scalar EPS(1E-4);
		
    for(Index i = 0; i < mat.cols(); ++i)
      {
        for(Index j = 0; j < i; ++j)
          {
            Scalar r = mat.col(i).dot(mat.col(j));
            mat.col(i) -= r * mat.col(j);
          }
			
        Scalar norm = mat.col(i).norm();
			
        if(norm < EPS)
          {
            for(Index k = i; k < mat.cols(); ++k)
              mat.col(k).setZero();
            return;
          }
        mat.col(i) /= norm;
      }
  }
	
  template<typename _MatrixType>
  class RedSVD
  {
  public:
    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    typedef typename Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> DenseMatrix;
    typedef typename Eigen::Matrix<Scalar, Eigen::Dynamic, 1> ScalarVector;
		
    RedSVD() {}
		
    RedSVD(const MatrixType& A, const Index rank, const int l)
    {
      compute(A, rank, l);
    }
		
    void compute(const MatrixType& A, const Index rank, const int l)
    {
      
      if(A.cols() == 0 || A.rows() == 0)
        return;
			
      Index r = (rank < A.cols()) ? rank : A.cols();
			
      r = (r < A.rows()) ? r : A.rows();
			
      // Gaussian Random Matrix for A^T
      DenseMatrix O(A.cols(), r+l);
      sample_gaussian(O);

      DenseMatrix M(A.rows(), r+l);
      DenseMatrix B(r+l, A.cols());

      // Power iteration
      for (int pow_iter=0; pow_iter<5; pow_iter++) {
        M = A * O;
        gram_schmidt(M);
        B = M.transpose() * A;
        O = B.transpose();
      }
			
      Eigen::JacobiSVD<DenseMatrix> svdOfB(B, Eigen::ComputeThinU | Eigen::ComputeThinV);
			
      m_matrixU = M * (svdOfB.matrixU().topLeftCorner(r+l, r));
      m_vectorS = svdOfB.singularValues().head(r);
      m_matrixV = svdOfB.matrixV().topLeftCorner(A.cols(), r);
    }
		
    DenseMatrix matrixU() const
    {
      return m_matrixU;
    }
		
    ScalarVector singularValues() const
    {
      return m_vectorS;
    }
		
    DenseMatrix matrixV() const
    {
      return m_matrixV;
    }
		
  private:
    DenseMatrix m_matrixU;
    ScalarVector m_vectorS;
    DenseMatrix m_matrixV;
  };
	
  template<typename _MatrixType>
  class RedSymEigen
  {
  public:
    typedef _MatrixType MatrixType;
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    typedef typename Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> DenseMatrix;
    typedef typename Eigen::Matrix<Scalar, Eigen::Dynamic, 1> ScalarVector;
		
    RedSymEigen() {}
		
    RedSymEigen(const MatrixType& A)
    {
      int r = (A.rows() < A.cols()) ? A.rows() : A.cols();
      compute(A, r);
    }
		
    RedSymEigen(const MatrixType& A, const Index rank)
    {
      compute(A, rank);
    }  

    void compute(const MatrixType& A, const Index rank)
    {
      if(A.cols() == 0 || A.rows() == 0)
        return;
      
      Index r = (rank < A.cols()) ? rank : A.cols();
      
      r = (r < A.rows()) ? r : A.rows();
      
      // Gaussian Random Matrix
      DenseMatrix O(A.rows(), r);
      sample_gaussian(O);

      DenseMatrix M(A.rows(), r);
      DenseMatrix B(r, A.cols());
      
      // Power iteration
      for (int pow_iter=0; pow_iter<3; pow_iter++) {
        M = A * O;
        gram_schmidt(M);
        B = M.transpose() * A;
        O = B.transpose();
      }
      
      Eigen::SelfAdjointEigenSolver<DenseMatrix> eigenOfB(B * M);
      
      m_eigenvalues = eigenOfB.eigenvalues();
      m_eigenvectors = M * eigenOfB.eigenvectors();
    }
    
    ScalarVector eigenvalues() const
    {
      return m_eigenvalues;
    }

    DenseMatrix eigenvectors() const
    {
      return m_eigenvectors;
    }

  private:
    ScalarVector m_eigenvalues;
    DenseMatrix m_eigenvectors;
  };

}

#endif

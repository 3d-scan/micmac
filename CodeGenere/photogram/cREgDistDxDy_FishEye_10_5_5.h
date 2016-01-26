// File Automatically generated by eLiSe
#include "StdAfx.h"


class cREgDistDxDy_FishEye_10_5_5: public cElCompiledFonc
{
   public :

      cREgDistDxDy_FishEye_10_5_5();
      void ComputeVal();
      void ComputeValDeriv();
      void ComputeValDerivHessian();
      double * AdrVarLocFromString(const std::string &);
      void SetFishEye_10_5_5_State_0_0(double);
      void SetRegDistxy1_x(double);
      void SetRegDistxy1_y(double);
      void SetRegDistxy2_x(double);
      void SetRegDistxy2_y(double);
      void SetRegDistxy3_x(double);
      void SetRegDistxy3_y(double);
      void SetRegDistxy4_x(double);
      void SetRegDistxy4_y(double);


      static cAutoAddEntry  mTheAuto;
      static cElCompiledFonc *  Alloc();
   private :

      double mLocFishEye_10_5_5_State_0_0;
      double mLocRegDistxy1_x;
      double mLocRegDistxy1_y;
      double mLocRegDistxy2_x;
      double mLocRegDistxy2_y;
      double mLocRegDistxy3_x;
      double mLocRegDistxy3_y;
      double mLocRegDistxy4_x;
      double mLocRegDistxy4_y;
};

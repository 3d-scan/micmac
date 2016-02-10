// File Automatically generated by eLiSe
#include "StdAfx.h"


class cREgDistDxx_EquiSolid_FishEye_10_5_5: public cElCompiledFonc
{
   public :

      cREgDistDxx_EquiSolid_FishEye_10_5_5();
      void ComputeVal();
      void ComputeValDeriv();
      void ComputeValDerivHessian();
      double * AdrVarLocFromString(const std::string &);
      void SetEquiSolid_FishEye_10_5_5_State_0_0(double);
      void SetRegDistuu1_x(double);
      void SetRegDistuu1_y(double);
      void SetRegDistuu2_x(double);
      void SetRegDistuu2_y(double);
      void SetRegDistuu3_x(double);
      void SetRegDistuu3_y(double);


      static cAutoAddEntry  mTheAuto;
      static cElCompiledFonc *  Alloc();
   private :

      double mLocEquiSolid_FishEye_10_5_5_State_0_0;
      double mLocRegDistuu1_x;
      double mLocRegDistuu1_y;
      double mLocRegDistuu2_x;
      double mLocRegDistuu2_y;
      double mLocRegDistuu3_x;
      double mLocRegDistuu3_y;
};

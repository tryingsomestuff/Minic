#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

template<Color> struct them_ {};
template<> struct them_<Co_White> { static constexpr Color value = Co_Black; };
template<> struct them_<Co_Black> { static constexpr Color value = Co_White; };

template<typename T, typename U> struct Sided {
   using returnType = U;

   T&       cast() { return static_cast<T&>(*this); }
   const T& cast() const { return static_cast<const T&>(*this); }

   template<Color c> returnType& us() {
      if constexpr (c == Co_White) { return cast().white; }
      else {
         return cast().black;
      }
   }

   template<Color c> const returnType& us() const {
      if constexpr (c == Co_White) { return cast().white; }
      else {
         return cast().black;
      }
   }

   template<Color c> returnType& them() { return us<them_<c>::value>(); }

   template<Color c> const returnType& them() const { return us<them_<c>::value>(); }

  private:
   Sided() {};
   friend T;
};

#endif // WITH_NNUE
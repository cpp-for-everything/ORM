/**
 *  @file   warnings.hpp
 *  @brief  Warning disabling macros for certain scenarios
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#pragma once

#if defined(_MSC_VER)
    #define DISABLE_WARNING_PUSH           __pragma(warning( push ))
    #define DISABLE_WARNING_PUSH3          __pragma(warning( push, 3 ))
    #define DISABLE_WARNING_POP            __pragma(warning( pop )) 
    #define DISABLE_WARNING(warningNumber) __pragma(warning( disable : warningNumber ))

    #define DISABLE_WARNING_UNREACHABLE_CODE    DISABLE_WARNING(4702)
    #define DISABLE_WARNING_LOSS_OF_DATA        DISABLE_WARNING(4244)
#elif defined(__GNUC__) || defined(__clang__)
    #define DO_PRAGMA(X) _Pragma(#X)
    #define DISABLE_WARNING_PUSH           DO_PRAGMA(GCC diagnostic push)
    #define DISABLE_WARNING_PUSH3          /**/
    #define DISABLE_WARNING_POP            DO_PRAGMA(GCC diagnostic pop) 
    #define DISABLE_WARNING(warningName)   DO_PRAGMA(GCC diagnostic ignored #warningName)

    #define DISABLE_WARNING_UNREACHABLE_CODE    DISABLE_WARNING(-Wunreachable-code)
    #define DISABLE_WARNING_LOSS_OF_DATA        DISABLE_WARNING(-Wconversion)
#else
    #define DISABLE_WARNING_PUSH               /**/
    #define DISABLE_WARNING_PUSH3              /**/
    #define DISABLE_WARNING_POP                /**/
    #define DISABLE_WARNING_UNREACHABLE_CODE   /**/
    #define DISABLE_WARNING_LOSS_OF_DATA       /**/

    #define DISABLE_WARNING_USER_DEFINED_COMMA /**/
#endif
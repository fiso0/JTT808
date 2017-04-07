/*
	ÐÞ¸ÄËµÃ÷£º
*/

/*****************************************************************************
*
* Filename:
* ---------
*   kal_general_types.h
*
* Project:
* --------
*   Maui_Software
*
* Description:
* ------------
*   This file provides general fundemental types definations.
*   Independent with underlaying RTOS.
*
*   User who include this file may not require KAL API at all.
*
* Author:
* -------
* -------
*
****************************************************************************/

#ifndef _KAL_GENERAL_TYPES_H
#define _KAL_GENERAL_TYPES_H

#include "stdlib.h" /*#include "clib.h"*/

/*******************************************************************************
* Type Definitions
*******************************************************************************/

/* portable character for multichar character set */
typedef char                    kal_char;
/* portable wide character for unicode character set */
typedef unsigned short          kal_wchar;

/* portable 8-bit unsigned integer */
typedef unsigned char           kal_uint8;
/* portable 8-bit signed integer */
typedef signed char             kal_int8;
/* portable 16-bit unsigned integer */
typedef unsigned short int      kal_uint16;
/* portable 16-bit signed integer */
typedef signed short int        kal_int16;
/* portable 32-bit unsigned integer */
typedef unsigned int            kal_uint32;
/* portable 32-bit signed integer */
typedef signed int              kal_int32;

#if !defined(GEN_FOR_PC) && !defined(__MTK_TARGET__)
/* portable 64-bit unsigned integer */
typedef unsigned __int64     kal_uint64;
/* portable 64-bit signed integer */
typedef __int64              kal_int64;
#else
/* portable 64-bit unsigned integer */
typedef unsigned long long   kal_uint64;
/* portable 64-bit signed integer */
typedef signed long long     kal_int64;
#endif

/* boolean representation */
typedef enum
{
	/* FALSE value */
	KAL_FALSE,
	/* TRUE value */
	KAL_TRUE
} kal_bool;

#if !defined(_WINNT_H) && !defined(_WINNT_)
typedef unsigned short WCHAR;
#endif

/*******************************************************************************
* Constant definition
*******************************************************************************/
#ifndef NULL
#define NULL               0
#endif

#endif  /* _KAL_GENERAL_TYPES_H */

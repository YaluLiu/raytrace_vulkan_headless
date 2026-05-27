//
// Copyright 2017 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef USDRAYSENSOR_API_H
#define USDRAYSENSOR_API_H

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define USDRAYSENSOR_API
#   define USDRAYSENSOR_API_TEMPLATE_CLASS(...)
#   define USDRAYSENSOR_API_TEMPLATE_STRUCT(...)
#   define USDRAYSENSOR_LOCAL
#else
#   if defined(USDRAYSENSOR_EXPORTS)
#       define USDRAYSENSOR_API ARCH_EXPORT
#       define USDRAYSENSOR_API_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define USDRAYSENSOR_API_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define USDRAYSENSOR_API ARCH_IMPORT
#       define USDRAYSENSOR_API_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define USDRAYSENSOR_API_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#   define USDRAYSENSOR_LOCAL ARCH_HIDDEN
#endif

#endif

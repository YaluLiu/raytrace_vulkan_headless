//
// Copyright 2017 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef HDROBOTLIDARUSD_API_H
#define HDROBOTLIDARUSD_API_H

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define HDROBOTLIDARUSD_API
#   define HDROBOTLIDARUSD_API_TEMPLATE_CLASS(...)
#   define HDROBOTLIDARUSD_API_TEMPLATE_STRUCT(...)
#   define HDROBOTLIDARUSD_LOCAL
#else
#   if defined(HDROBOTLIDARUSD_EXPORTS)
#       define HDROBOTLIDARUSD_API ARCH_EXPORT
#       define HDROBOTLIDARUSD_API_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define HDROBOTLIDARUSD_API_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define HDROBOTLIDARUSD_API ARCH_IMPORT
#       define HDROBOTLIDARUSD_API_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define HDROBOTLIDARUSD_API_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#   define HDROBOTLIDARUSD_LOCAL ARCH_HIDDEN
#endif

#endif

/******************************************************************************
** Copyright (c) 2016-2018, Intel Corporation                                **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Alexander Heinecke, Kunal Banerjee (Intel Corp.)
******************************************************************************/

#include <libxsmm.h>
#include <math.h>
#include "libxsmm_main.h"

#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(push,target(LIBXSMM_OFFLOAD_TARGET))
#endif
#include <string.h>
#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(pop)
#endif

#if defined(LSTM_TIMING)
#include <stdio.h>
double Gbl_t_input_total = 0., Gbl_t_recur_total = 0., Gbl_t_eltwise_total = 0., Gbl_t_nonlin_total = 0.;
unsigned long long Gbl_t_input = 0, Gbl_t_recur = 0, Gbl_t_eltwise = 0, Gbl_t_nonlin = 0;
double Gbl_duration_input = 0., Gbl_duration_recur = 0., Gbl_duration_eltwise = 0., Gbl_duration_nonlin = 0.;
#endif


LIBXSMM_API libxsmm_dnn_lstmcell* libxsmm_dnn_create_lstmcell(libxsmm_dnn_lstmcell_desc lstmcell_desc, libxsmm_dnn_err_t* status) {
  libxsmm_dnn_lstmcell* handle = 0;
  *status = LIBXSMM_DNN_SUCCESS;

  handle = (libxsmm_dnn_lstmcell*)malloc(sizeof(libxsmm_dnn_lstmcell));
  if (0 != handle) {
    /* zero entire content; not only safer but also sets data and code pointers to NULL */
    memset(handle, 0, sizeof(*handle));
    /* initialize known handle components */
    handle->desc = lstmcell_desc;
    handle->datatype_in = lstmcell_desc.datatype_in;
    handle->datatype_out = lstmcell_desc.datatype_out;
    if ( (lstmcell_desc.datatype_in != LIBXSMM_DNN_DATATYPE_F32) || (lstmcell_desc.datatype_out != LIBXSMM_DNN_DATATYPE_F32) ) {
      /* error */
      *status = LIBXSMM_DNN_ERR_UNSUPPORTED_DATATYPE;
      return handle;
    }
    handle->buffer_format = lstmcell_desc.buffer_format;
    handle->m = lstmcell_desc.m;
    handle->n = lstmcell_desc.n;
    handle->k = lstmcell_desc.k;
    handle->t = lstmcell_desc.t;
    if (lstmcell_desc.t < 2) {
      *status = LIBXSMM_DNN_ERR_TIME_STEPS_TOO_SMALL;
    }
    handle->bm = lstmcell_desc.bm;
    handle->bn = lstmcell_desc.bn;
    handle->bk = lstmcell_desc.bk;
    handle->b_m1 = lstmcell_desc.b_m1;
    handle->b_n1 = lstmcell_desc.b_n1;
    handle->b_k1 = lstmcell_desc.b_k1;
    handle->b_m2 = lstmcell_desc.b_m2;
    handle->b_n2 = lstmcell_desc.b_n2;
    handle->b_k2 = lstmcell_desc.b_k2;
  } else {
    *status = LIBXSMM_DNN_ERR_CREATE_HANDLE;
  }
  return handle;
}


LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_destroy_lstmcell(const libxsmm_dnn_lstmcell* handle) {
  libxsmm_dnn_err_t status = LIBXSMM_DNN_SUCCESS;
  if (0 != handle) {
    /* deallocate handle structure */
    free(/*remove constness*/(libxsmm_dnn_lstmcell*)handle);
  }
  return status;
}


LIBXSMM_API libxsmm_dnn_tensor_datalayout* libxsmm_dnn_lstmcell_create_tensor_datalayout(const libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_tensor_type type, libxsmm_dnn_err_t* status) {
  libxsmm_dnn_tensor_datalayout* layout = 0;
  *status = LIBXSMM_DNN_SUCCESS;
  layout = 0;
  if (handle != 0) {
    layout = (libxsmm_dnn_tensor_datalayout*) malloc(sizeof(libxsmm_dnn_tensor_datalayout));
    if (layout != 0) {
      memset(layout, 0, sizeof(libxsmm_dnn_tensor_datalayout));
      /*layout->custom_format = handle->custom_format_type;*/
      if ( (type == LIBXSMM_DNN_REGULAR_INPUT)  || (type == LIBXSMM_DNN_GRADIENT_INPUT)  || (type == LIBXSMM_DNN_INPUT)  ||
           (type == LIBXSMM_DNN_REGULAR_OUTPUT) || (type == LIBXSMM_DNN_GRADIENT_OUTPUT) || (type == LIBXSMM_DNN_OUTPUT)    ) {
        layout->format = handle->buffer_format;
        layout->tensor_type = LIBXSMM_DNN_ACTIVATION;

        if ((handle->buffer_format & LIBXSMM_DNN_TENSOR_FORMAT_LIBXSMM) > 0) {
          if ( ((handle->datatype_in == LIBXSMM_DNN_DATATYPE_F32) && (handle->datatype_out == LIBXSMM_DNN_DATATYPE_F32) ) ) {
            layout->datatype = LIBXSMM_DNN_DATATYPE_F32;
            if (1 /*handle->custom_format_type == LIBXSMM_DNN_TENSOR_FORMAT_LIBXSMM_1*/) {
              layout->dim_type = (libxsmm_dnn_tensor_dimtype*) malloc(4*sizeof(libxsmm_dnn_tensor_dimtype));
              layout->dim_size = (unsigned int*) malloc(4*sizeof(unsigned int));

              if (0 != layout->dim_type && 0 != layout->dim_size) { /* TODO: handle the error */
                layout->num_dims = 4;
                layout->dim_type[0] = LIBXSMM_DNN_TENSOR_DIMTYPE_C;
                layout->dim_type[1] = LIBXSMM_DNN_TENSOR_DIMTYPE_C;
                layout->dim_type[2] = LIBXSMM_DNN_TENSOR_DIMTYPE_W;
                layout->dim_type[3] = LIBXSMM_DNN_TENSOR_DIMTYPE_H;
                if ( (type == LIBXSMM_DNN_REGULAR_INPUT) || (type == LIBXSMM_DNN_GRADIENT_INPUT) || (type == LIBXSMM_DNN_INPUT) ) {
                  layout->dim_size[0] = handle->bm;
                  layout->dim_size[1] = handle->bk;
                  layout->dim_size[2] = handle->k / handle->bk;
                  layout->dim_size[3] = handle->m / handle->bm;
                } else if ( (type == LIBXSMM_DNN_REGULAR_OUTPUT) || (type == LIBXSMM_DNN_GRADIENT_OUTPUT) || (type == LIBXSMM_DNN_OUTPUT) ) {
                  layout->dim_size[0] = handle->bm;
                  layout->dim_size[1] = handle->bn;
                  layout->dim_size[2] = handle->m / handle->bm;
                  layout->dim_size[3] = handle->n / handle->bn;
                } else {
                  free(layout->dim_type);
                  free(layout->dim_size);
                  free(layout);
                  layout = 0; /* make sure a NULL is returned */
                  *status = LIBXSMM_DNN_ERR_UNKNOWN_TENSOR_TYPE;
                }
              }
            } else {
              free(layout);
              layout = 0; /* make sure a NULL is returned */
              *status = LIBXSMM_DNN_ERR_UNKNOWN_TENSOR_TYPE;
            }
          } else {
            free(layout);
            layout = 0; /* make sure a NULL is returned */
            *status = LIBXSMM_DNN_ERR_UNSUPPORTED_DATATYPE;
          }
        } else {
          free(layout);
          layout = 0; /* make sure a NULL is returned */
          *status = LIBXSMM_DNN_ERR_INVALID_FORMAT_GENERAL;
        }
      } else {
        free(layout);
        layout = 0; /* make sure a NULL is returned */
        *status = LIBXSMM_DNN_ERR_UNKNOWN_TENSOR_TYPE;
      }
    } else {
      *status = LIBXSMM_DNN_ERR_CREATE_LAYOUT;
    }
  } else {
    *status = LIBXSMM_DNN_ERR_INVALID_HANDLE;
  }
  return layout;
}


LIBXSMM_API size_t libxsmm_dnn_lstmcell_get_scratch_size(const libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_compute_kind kind, libxsmm_dnn_err_t* status) {
  size_t sizeof_datatype = sizeof(float);
  size_t size = 0;
  *status = LIBXSMM_DNN_SUCCESS;

  if (0 != handle) {
    switch (kind) {
      case LIBXSMM_DNN_COMPUTE_KIND_FWD: {
#if defined(NON_FUSED_INPUT_GEMM)
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* i1t */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* f1t */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* o1t */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* c1t */
                                           size += 64;
#else
                                           size += handle->m * 4 * handle->n * sizeof_datatype * handle->t; /* i1t */
                                           size += 64;
                                           size += 0; /* f1t */
                                           size += 0; /* o1t */
                                           size += 0; /* c1t */
#endif
                                           size += handle->m * handle->n * sizeof_datatype; /* i2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* f2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* o2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* c2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* d1 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* d2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* dh */
                                           size += 64;
                                         } break;
      case LIBXSMM_DNN_COMPUTE_KIND_BWD:
      case LIBXSMM_DNN_COMPUTE_KIND_UPD:
      case LIBXSMM_DNN_COMPUTE_KIND_ALL: {
#if defined(NON_FUSED_INPUT_GEMM)
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* i1t */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* f1t */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* o1t */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype * handle->t; /* c1t */
                                           size += 64;
#else
                                           size += handle->m * 4 * handle->n * sizeof_datatype * handle->t; /* i1t */
                                           size += 64;
                                           size += 0; /* f1t */
                                           size += 0; /* o1t */
                                           size += 0; /* c1t */
#endif
                                           size += handle->m * handle->n * sizeof_datatype; /* i1b */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* i2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* i3 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* f1b */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* f2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* f3 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* o1b */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* o2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* c1b */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* c2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* d1 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* d2 */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* d3 (dh) */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* d4 */
                                           size += 64;
                                           size += handle->m * handle->k * sizeof_datatype; /* wTp */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* rTp */
                                           size += 64;
                                           size += handle->k * handle->n * sizeof_datatype; /* xTp */
                                           size += 64;
                                           size += handle->m * handle->n * sizeof_datatype; /* deltaTp */
                                           size += 64;
                                         } break;
      default: {
                 *status = LIBXSMM_DNN_ERR_INVALID_KIND;
               }
    }
  } else {
    *status = LIBXSMM_DNN_ERR_INVALID_HANDLE;
  }

  return size;
}


LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_lstmcell_bind_scratch(libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_compute_kind kind, const void* scratch) {
  libxsmm_dnn_err_t status = LIBXSMM_DNN_SUCCESS;
  size_t address = (size_t)scratch;
  size_t offset = 0;
  size_t scratch_size = 0;
  size_t sizeof_datatype = sizeof(float);

  if (scratch == 0) {
    status = LIBXSMM_DNN_ERR_SCRATCH_NOT_ALLOCED;
    return status;
  }

  if (0 != handle) {
    switch (kind) {
      case LIBXSMM_DNN_COMPUTE_KIND_FWD: {
#if defined(NON_FUSED_INPUT_GEMM)
                                           if (address % 64 == 0) {
                                             handle->i1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->f1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->f1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->o1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->o1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->c1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->c1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
#else
                                           if (address % 64 == 0) {
                                             handle->i1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i1t->data = (void*)(address+offset);
                                           }
                                           handle->f1t->data = handle->i1t->data; /* not used */
                                           handle->o1t->data = handle->i1t->data; /* not used */
                                           handle->c1t->data = handle->i1t->data; /* not used */
                                           scratch_size = handle->m * 4 * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
#endif
                                           if (address % 64 == 0) {
                                             handle->i2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->f2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->f2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->o2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->o2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->c2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->c2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->d1->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->d1->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->d2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->d2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->dh->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->dh->data = (void*)(address+offset);
                                           }
                                         } break;
      case LIBXSMM_DNN_COMPUTE_KIND_BWD:
      case LIBXSMM_DNN_COMPUTE_KIND_UPD:
      case LIBXSMM_DNN_COMPUTE_KIND_ALL: {
#if defined(NON_FUSED_INPUT_GEMM)
                                           if (address % 64 == 0) {
                                             handle->i1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->f1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->f1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->o1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->o1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->c1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->c1t->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
#else
                                           if (address % 64 == 0) {
                                             handle->i1t->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i1t->data = (void*)(address+offset);
                                           }
                                           handle->f1t->data = handle->i1t->data; /* not used */
                                           handle->o1t->data = handle->i1t->data; /* not used */
                                           handle->c1t->data = handle->i1t->data; /* not used */
                                           scratch_size = handle->m * 4 * handle->n * sizeof_datatype * handle->t;
                                           address += scratch_size + 64;
#endif
                                           if (address % 64 == 0) {
                                             handle->i1b->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i1b->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->i2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->i3->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->i3->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->f1b->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->f1b->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->f2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->f2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->f3->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->f3->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->o1b->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->o1b->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->o2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->o2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->c1b->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->c1b->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->c2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->c2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->d1->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->d1->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->d2->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->d2->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->dh->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->dh->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->d4->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->d4->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->wTp->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->wTp->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->k * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->rTp->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->rTp->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->m * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->xTp->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->xTp->data = (void*)(address+offset);
                                           }
                                           scratch_size = handle->k * handle->n * sizeof_datatype;
                                           address += scratch_size + 64;
                                           if (address % 64 == 0) {
                                             handle->deltaTp->data = (void*)address;
                                           } else {
                                             offset = (64 - address % 64);
                                             handle->deltaTp->data = (void*)(address+offset);
                                           }
                                         } break;
      default: {
                 status = LIBXSMM_DNN_ERR_INVALID_KIND;
               }
    }
  } else {
    status = LIBXSMM_DNN_ERR_INVALID_HANDLE;
  }

  return status;
}


LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_lstmcell_release_scratch(libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_compute_kind kind) {
  libxsmm_dnn_err_t status = LIBXSMM_DNN_SUCCESS;

  if (0 != handle) {
    switch (kind) {
      case LIBXSMM_DNN_COMPUTE_KIND_FWD: {
                                           handle->i1t->data = 0;
                                           handle->i2->data = 0;
                                           handle->f1t->data = 0;
                                           handle->f2->data = 0;
                                           handle->o1t->data = 0;
                                           handle->o2->data = 0;
                                           handle->c1t->data = 0;
                                           handle->c2->data = 0;
                                           handle->d1->data = 0;
                                           handle->d2->data = 0;
                                           handle->dh->data = 0;
                                           handle->i1t = 0;
                                           handle->i2 = 0;
                                           handle->f1t = 0;
                                           handle->f2 = 0;
                                           handle->o1t = 0;
                                           handle->o2 = 0;
                                           handle->c1t = 0;
                                           handle->c2 = 0;
                                           handle->d1 = 0;
                                           handle->d2 = 0;
                                           handle->dh = 0;
                                         } break;
      case LIBXSMM_DNN_COMPUTE_KIND_BWD:
      case LIBXSMM_DNN_COMPUTE_KIND_UPD:
      case LIBXSMM_DNN_COMPUTE_KIND_ALL: {
                                           handle->i1t->data = 0;
                                           handle->i1b->data = 0;
                                           handle->i2->data = 0;
                                           handle->f1t->data = 0;
                                           handle->f1b->data = 0;
                                           handle->f2->data = 0;
                                           handle->o1t->data = 0;
                                           handle->o1b->data = 0;
                                           handle->o2->data = 0;
                                           handle->c1t->data = 0;
                                           handle->c1b->data = 0;
                                           handle->c2->data = 0;
                                           handle->d1->data = 0;
                                           handle->d2->data = 0;
                                           handle->dh->data = 0;
                                           handle->i3->data = 0;
                                           handle->f3->data = 0;
                                           handle->d4->data = 0;
                                           handle->rTp->data = 0;
                                           handle->wTp->data = 0;
                                           handle->xTp->data = 0;
                                           handle->deltaTp->data = 0;
                                           handle->i1t = 0;
                                           handle->i1b = 0;
                                           handle->i2 = 0;
                                           handle->f1t = 0;
                                           handle->f1b = 0;
                                           handle->f2 = 0;
                                           handle->o1t = 0;
                                           handle->o1b = 0;
                                           handle->o2 = 0;
                                           handle->c1t = 0;
                                           handle->c1b = 0;
                                           handle->c2 = 0;
                                           handle->d1 = 0;
                                           handle->d2 = 0;
                                           handle->dh = 0;
                                           handle->i3 = 0;
                                           handle->f3 = 0;
                                           handle->d4 = 0;
                                           handle->rTp = 0;
                                           handle->wTp = 0;
                                           handle->xTp = 0;
                                           handle->deltaTp = 0;
                                         } break;
      default: {
                 status = LIBXSMM_DNN_ERR_INVALID_KIND;
               }
    }
  } else {
    status = LIBXSMM_DNN_ERR_INVALID_HANDLE;
  }

  return status;
}


LIBXSMM_API size_t libxsmm_dnn_lstmcell_get_internalstate_size(const libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_compute_kind kind, libxsmm_dnn_err_t* status) {
  size_t size = 0;

  LIBXSMM_UNUSED( handle );
  LIBXSMM_UNUSED( kind );
  *status = LIBXSMM_DNN_SUCCESS;

  return size;
}


LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_lstmcell_bind_internalstate(libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_compute_kind kind, const void* internalstate) {
  LIBXSMM_UNUSED( handle );
  LIBXSMM_UNUSED( kind );
  LIBXSMM_UNUSED( internalstate );

  return LIBXSMM_DNN_SUCCESS;
}


LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_lstmcell_release_internalstate(libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_compute_kind kind) {
  LIBXSMM_UNUSED( handle );
  LIBXSMM_UNUSED( kind );

  return LIBXSMM_DNN_SUCCESS;
}


LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_lstmcell_bind_tensor(libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_tensor* tensor, const libxsmm_dnn_tensor_type type) {
  LIBXSMM_UNUSED( handle );
  LIBXSMM_UNUSED( tensor );
  LIBXSMM_UNUSED( type );

  return LIBXSMM_DNN_SUCCESS;
}


LIBXSMM_API libxsmm_dnn_tensor* libxsmm_dnn_lstmcell_get_tensor(libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_tensor_type type, libxsmm_dnn_err_t* status) {
  libxsmm_dnn_tensor* tensor = 0;

  LIBXSMM_UNUSED( handle );
  LIBXSMM_UNUSED( type );
  *status = LIBXSMM_DNN_SUCCESS;

  return tensor;
}

LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_lstmcell_release_tensor(libxsmm_dnn_lstmcell* handle, const libxsmm_dnn_tensor_type type) {
  LIBXSMM_UNUSED( handle );
  LIBXSMM_UNUSED( type );

  return LIBXSMM_DNN_SUCCESS;
}


LIBXSMM_API libxsmm_dnn_err_t libxsmm_dnn_lstmcell_execute_st(libxsmm_dnn_lstmcell* handle, libxsmm_dnn_compute_kind kind,
  /*unsigned*/int start_thread, /*unsigned*/int tid) {
  LIBXSMM_UNUSED( handle );
  LIBXSMM_UNUSED( kind );
  LIBXSMM_UNUSED( start_thread );
  LIBXSMM_UNUSED( tid );

  return LIBXSMM_DNN_SUCCESS;
}


/*
Copyright (©) 2024  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "sanitiser.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __x86_64__
#include <arch/x86_64/ArchDefs.h>
#endif

struct ubsan_source_location {
    const char* filename;
    uint32_t line;
    uint32_t column;
};

struct ubsan_type_descriptor {
    uint16_t type_kind;
    uint16_t type_info;
    char type_name[];
};

typedef struct ubsan_source_location ubsan_source_location;
typedef struct ubsan_type_descriptor ubsan_type_descriptor;

struct ubsan_overflow_data {
    ubsan_source_location location;
    ubsan_type_descriptor* type;
};

struct ubsan_shift_out_of_bounds_data {
    ubsan_source_location location;
    ubsan_type_descriptor* lhs_type;
    ubsan_type_descriptor* rhs_type;
};

struct ubsan_out_of_bounds_data {
    ubsan_source_location location;
    ubsan_type_descriptor* array_type;
    ubsan_type_descriptor* index_type;
};

struct ubsan_invalid_value_data {
    ubsan_source_location location;
    ubsan_type_descriptor* type;
};

struct ubsan_unreachable_data {
    ubsan_source_location location;
};

struct ubsan_vla_bound_data {
    ubsan_source_location location;
    ubsan_type_descriptor* type;
};

struct ubsan_type_mismatch_v1_data {
    ubsan_source_location location;
    ubsan_type_descriptor* type;
    unsigned char log_alignment;
    unsigned char type_check_kind;
};

struct ubsan_alignment_data {
    ubsan_source_location location;
    ubsan_type_descriptor* type;
    unsigned char log_alignment;
    unsigned char type_check_kind;
};

struct ubsan_nonnull_arg_data {
    ubsan_source_location location;
};

struct ubsan_nonnull_return_data {
    ubsan_source_location location;
};

struct ubsan_nonnull_return_v1_data {
    ubsan_source_location location;
};

struct ubsan_missing_return_data {
    ubsan_source_location location;
};

typedef struct ubsan_overflow_data ubsan_overflow_data;
typedef struct ubsan_shift_out_of_bounds_data ubsan_shift_out_of_bounds_data;
typedef struct ubsan_out_of_bounds_data ubsan_out_of_bounds_data;
typedef struct ubsan_invalid_value_data ubsan_invalid_value_data;
typedef struct ubsan_unreachable_data ubsan_unreachable_data;
typedef struct ubsan_vla_bound_data ubsan_vla_bound_data;
typedef struct ubsan_type_mismatch_v1_data ubsan_type_mismatch_v1_data;
typedef struct ubsan_alignment_data ubsan_alignment_data;
typedef struct ubsan_nonnull_arg_data ubsan_nonnull_arg_data;
typedef struct ubsan_nonnull_return_data ubsan_nonnull_return_data;
typedef struct ubsan_nonnull_return_v1_data ubsan_nonnull_return_v1_data;
typedef struct ubsan_missing_return_data ubsan_missing_return_data;

#ifdef __cplusplus
extern "C" {
#endif

void ubsan_print_error(const char* error, ubsan_source_location* location) {
#if defined(__x86_64__) && defined(_FROSTYOS_BUILD_TARGET_IS_KERNEL)
    x86_64_DisableInterrupts();
#endif
    char buffer[1024];
    snprintf(buffer, 1024, "UBSan: %s at %s:%d:%d\n", error, location->filename, location->line, location->column);
    sanitiser_panic(buffer);
}

void __ubsan_handle_add_overflow(ubsan_overflow_data* data) {
    ubsan_print_error("addition overflow", &data->location);
}

void __ubsan_handle_sub_overflow(ubsan_overflow_data* data) {
    ubsan_print_error("subtraction overflow", &data->location);
}

void __ubsan_handle_mul_overflow(ubsan_overflow_data* data) {
    ubsan_print_error("multiplication overflow", &data->location);
}

void __ubsan_handle_divrem_overflow(ubsan_overflow_data* data) {
    ubsan_print_error("division overflow", &data->location);
}

void __ubsan_handle_negate_overflow(ubsan_overflow_data* data) {
    ubsan_print_error("negation overflow", &data->location);
}

void __ubsan_handle_pointer_overflow(ubsan_overflow_data* data) {
    ubsan_print_error("pointer overflow", &data->location);
}

void __ubsan_handle_shift_out_of_bounds(ubsan_shift_out_of_bounds_data* data) {
    ubsan_print_error("shift out of bounds", &data->location);
}

void __ubsan_handle_load_invalid_value(ubsan_invalid_value_data* data, uintptr_t value) {
    ubsan_print_error("load invalid value", &data->location);
}

void __ubsan_handle_out_of_bounds(ubsan_out_of_bounds_data* data, uintptr_t index) {
    ubsan_print_error("out of bounds", &data->location);
}

void __ubsan_handle_type_mismatch_v1(ubsan_type_mismatch_v1_data* data, uintptr_t ptr) {
    dbgprintf("type mismatch: type = %s, alignment = %u, pointer = %lp\n", data->type->type_name, data->log_alignment, ptr);
    ubsan_print_error("type mismatch", &data->location);
}

void __ubsan_handle_vla_bound_not_positive(ubsan_vla_bound_data* data, uintptr_t bound) {
    ubsan_print_error("vla bound not positive", &data->location);
}

void __ubsan_handle_alignment_assumption(ubsan_alignment_data* data, uintptr_t ptr) {
    ubsan_print_error("alignment assumption", &data->location);
}

void __ubsan_handle_nonnull_return(ubsan_nonnull_return_data* data) {
    ubsan_print_error("nonnull return", &data->location);
}

void __ubsan_handle_nonnull_arg(ubsan_nonnull_arg_data* data) {
    ubsan_print_error("nonnull argument", &data->location);
}

void __ubsan_handle_builtin_unreachable(ubsan_unreachable_data* data) {
    ubsan_print_error("unreachable", &data->location);
}

void __ubsan_handle_invalid_builtin(ubsan_invalid_value_data* data, uintptr_t value) {
    ubsan_print_error("invalid builtin", &data->location);
}

void __ubsan_handle_missing_return(ubsan_missing_return_data* data) {
    ubsan_print_error("missing return", &data->location);
}

#ifdef __cplusplus
}
#endif

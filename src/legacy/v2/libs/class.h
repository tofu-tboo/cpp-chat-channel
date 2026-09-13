#ifndef __CLASS_H__
#define __CLASS_H__

#include <algorithm>

#include "../dynamic_compile/dynamic_compile.h"
__DEF_SEQ_TARGET__

#define forward_public			public
#define forward_protected		protected
#define forward_private			private

#define type_public				public
#define type_protected			protected
#define type_private			private

#define static_var_public		public
#define static_var_protected	protected
#define static_var_private		private

#define static_func_public		public
#define static_func_protected	protected
#define static_func_private		private

#define var_public				public
#define var_protected			protected
#define var_private				private

#define func_public				public
#define func_protected			protected
#define func_private			private

__DEF_SEQ_TARGET_END__

#define USING_TYPENAME(type, ...)	using type = typename __VA_ARGS__::type;

template<size_t N>
struct FixedString {
    char value[N];
    constexpr FixedString(const char (&str)[N]) { std::copy_n(str, N, value); }
};

#endif
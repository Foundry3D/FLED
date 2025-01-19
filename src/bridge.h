#include "luau_runtime.h"

namespace luau {
    class Bridge {
        public:
        void Init(Runtime* runtime);
        protected:
        friend class Runtime;
    };
};
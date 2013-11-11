#ifndef VEXCL_DEVLIST_HPP
#define VEXCL_DEVLIST_HPP

/*
The MIT License

Copyright (c) 2012-2013 Denis Demidov <ddemidov@ksu.ru>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/**
 * \file   devlist.hpp
 * \author Denis Demidov <ddemidov@ksu.ru>
 * \brief  OpenCL device enumeration and context initialization.
 */

#include <vector>
#include <functional>
#include <string>
#include <cstdlib>

#include <vexcl/backend.hpp>
#include <vexcl/util.hpp>

#ifdef __GNUC__
#  ifndef _GLIBCXX_USE_NANOSLEEP
#    define _GLIBCXX_USE_NANOSLEEP
#  endif
#endif

namespace vex {

/// Device filters.
namespace Filter {
    /// Selects any device.
    struct AnyFilter {
        bool operator()(const backend::device&) const {
            return true;
        }
    };

    const AnyFilter All = {};
    const AnyFilter Any = {};

    /// Selects no more than given number of devices.
    /**
     * \note This filter should be the last in filter expression. In this case,
     * it will be applied only to devices which passed all other filters.
     * Otherwise, you could get less devices than planned (every time this
     * filter is applied, internal counter is decremented).
     */
    struct Count {
        explicit Count(int c) : count(c) {}

        bool operator()(const backend::device&) const {
            return --count >= 0;
        }

        private:
            mutable int count;
    };

    /// Selects one device at the given position.
    /**
     * Select one device at the given position in the list of devices
     * satisfying previously applied filters.
     */
    struct Position {
        explicit Position(int p) : pos(p) {}

        bool operator()(const backend::device&) const {
            return 0 == pos--;
        }

        private:
            mutable int pos;
    };

    /// \cond INTERNAL

    /// Negation of a filter.
    struct NegateFilter {
        template <class Filter>
        NegateFilter(Filter&& filter)
          : filter(std::forward<Filter>(filter)) {}

        bool operator()(const backend::device &d) const {
            return !filter(d);
        }

        private:
            std::function<bool(const backend::device&)> filter;
    };

    /// Filter join operators.
    enum FilterOp {
        FilterAnd, FilterOr
    };

    /// Filter join expression template.
    template <FilterOp op>
    struct FilterBinaryOp {
        template <class LHS, class RHS>
        FilterBinaryOp(LHS&& lhs, RHS&& rhs)
            : lhs(std::forward<LHS>(lhs)), rhs(std::forward<RHS>(rhs)) {}

        bool operator()(const backend::device &d) const {
            // This could be hidden into FilterOp::apply() call (with FilterOp
            // being a struct instead of enum), but this form allows to rely on
            // short-circuit evaluation (important for mutable filters as Count
            // or Position).
            switch (op) {
                case FilterOr:
                    return lhs(d) || rhs(d);
                case FilterAnd:
                default:
                    return lhs(d) && rhs(d);
            }
        }

        private:
            std::function<bool(const backend::device&)> lhs;
            std::function<bool(const backend::device&)> rhs;
    };

    /// \endcond

    /// Join two filters with AND operator.
    template <class LHS, class RHS>
    FilterBinaryOp<FilterAnd> operator&&(LHS&& lhs, RHS&& rhs)
    {
        return FilterBinaryOp<FilterAnd>(std::forward<LHS>(lhs), std::forward<RHS>(rhs));
    }

    /// Join two filters with OR operator.
    template <class LHS, class RHS>
    FilterBinaryOp<FilterOr> operator||(LHS&& lhs, RHS&& rhs)
    {
        return FilterBinaryOp<FilterOr>(std::forward<LHS>(lhs), std::forward<RHS>(rhs));
    }

    /// Negate a filter.
    template <class Filter>
    NegateFilter operator!(Filter&& filter) {
        return NegateFilter(std::forward<Filter>(filter));
    }

    /// Runtime filter holder.
    /**
     * The filter can be changed at runtime as in:
     * \code
     * vex::Filter::General f = vex::Filter::Env;
     * if (need_double) f = f && vex::Filter::DoublePrecision;
     * \endcode
     */
    struct General {
        template<class Filter>
        General(Filter filter) : filter(filter) { }

        bool operator()(const backend::device &d) const {
            return filter(d);
        }

        private:
            std::function<bool(const backend::device&)> filter;
    };

    /// Environment filter
    /**
     * Selects devices with respect to environment variables. Recognized
     * variables are:
     *
     * \li OCL_PLATFORM -- platform name;
     * \li OCL_VENDOR   -- device vendor;
     * \li OCL_DEVICE   -- device name;
     * \li OCL_TYPE     -- device type (CPU, GPU, ACCELERATOR);
     * \li OCL_MAX_DEVICES -- maximum number of devices to use.
     * \li OCL_POSITION -- devices position in the device list.
     *
     * \note Since this filter possibly counts passed devices, it should be the
     * last in filter expression. Same reasoning applies as in case of
     * Filter::Count.
     */
    struct EnvFilter {
        EnvFilter()
            : filter( backend_env_filters() )
        {
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4996)
#endif
            const char *maxdev   = getenv("OCL_MAX_DEVICES");
            const char *position = getenv("OCL_POSITION");
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

            if (maxdev)   filter.push_back(Count(std::stoi(maxdev)));
            if (position) filter.push_back(Position(std::stoi(position)));
        }

        bool operator()(const backend::device &d) const {
            if (filter.empty()) return true;

            return std::all_of(
                    filter.begin(), filter.end(),
                    [d](const std::function<bool(const backend::device)> &f) {
                        return f(d);
                    });
        }

        private:
            std::vector< std::function<bool(const backend::device&)> > filter;
    };

    const EnvFilter Env;

} // namespace Filter

class Context;

template <bool dummy = true>
class StaticContext {
    static_assert(dummy, "dummy parameter should be true");

    public:
        static void set(Context &ctx) {
            instance = &ctx;
        }

        static const Context& get() {
            precondition(instance, "Uninitialized static context");
            return *instance;
        }
    private:
        static Context *instance;
};

template <bool dummy>
Context* StaticContext<dummy>::instance = 0;

/// Returns reference to the latest instance of vex::Context.
inline const Context& current_context() {
    return StaticContext<>::get();
}

/// VexCL context holder.
/**
 * Holds vectors of backend::contexts and backend::command_queues returned by queue_list.
 */
class Context {
    public:
        /// Initialize context from a device filter.
        template <class DevFilter>
        explicit Context(
                DevFilter&& filter, cl_command_queue_properties properties = 0
                )
        {
            std::tie(c, q) = backend::queue_list(std::forward<DevFilter>(filter), properties);

#ifdef VEXCL_THROW_ON_EMPTY_CONTEXT
            precondition(!q.empty(), "No compute devices found");
#endif

            StaticContext<>::set(*this);
        }

        /// Initializes context from user-supplied list of backend::contexts and backend::command_queues.
        Context(const std::vector<std::pair<backend::context, backend::command_queue>> &user_ctx) {
            c.reserve(user_ctx.size());
            q.reserve(user_ctx.size());
            for(auto u = user_ctx.begin(); u != user_ctx.end(); u++) {
                c.push_back(u->first);
                q.push_back(u->second);
            }

            StaticContext<>::set(*this);
        }

        const std::vector<backend::context>& context() const {
            return c;
        }

        const backend::context& context(unsigned d) const {
            return c[d];
        }

        const std::vector<backend::command_queue>& queue() const {
            return q;
        }

        operator const std::vector<backend::command_queue>&() const {
            return q;
        }

        const backend::command_queue& queue(unsigned d) const {
            return q[d];
        }

        size_t size() const {
            return q.size();
        }

        bool empty() const {
            return q.empty();
        }

        operator bool() const {
            return !empty();
        }

        void finish() const {
            for(auto queue = q.begin(); queue != q.end(); ++queue)
                queue->finish();
        }
    private:
        std::vector<backend::context>       c;
        std::vector<backend::command_queue> q;
};

} // namespace vex

namespace std {

/// Output list of devices to stream.
inline std::ostream& operator<<(std::ostream &os,
        const std::vector<vex::backend::command_queue> &queue)
{
    unsigned p = 0;

    for(auto q = queue.begin(); q != queue.end(); q++)
        os << ++p << ". " << *q << std::endl;

    return os;
}

/// Output list of devices to stream.
inline std::ostream& operator<<(std::ostream &os, const vex::Context &ctx) {
    return os << ctx.queue();
}

}

#endif

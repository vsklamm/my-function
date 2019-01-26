#pragma once

#include <memory>

namespace vsklamm
{
	constexpr size_t small_object_size = 32;

	template <typename Signature>
	struct function;

	template <typename ReturnType, typename ...Args>
	struct function <ReturnType(Args...)>
	{

		function() noexcept : f_pointer_(nullptr), aligned_ptr(raw_object), is_small_(false) {}

		function(std::nullptr_t) noexcept : f_pointer_(nullptr), aligned_ptr(raw_object), is_small_(false) {}

		template <typename FunctionT>
		function(FunctionT f)
		{
			// -sanitize=address, undefined
			aligned_ptr = raw_object;		

			void * p = reinterpret_cast<void *>(raw_object);
			size_t space = small_object_size;
			std::align(alignof(function_holder<FunctionT>), sizeof(function_holder<FunctionT>), p, space);

			if (p != nullptr && std::is_nothrow_move_constructible<FunctionT>::value && sizeof(function_holder<FunctionT>) <= small_object_size)
			{
				aligned_ptr = reinterpret_cast<char *>(p);
				new (aligned_ptr) function_holder<FunctionT>(std::move(f));
				is_small_ = true;
			}
			else
			{
				new (aligned_ptr) std::unique_ptr<function_holder_base>(std::make_unique<function_holder<FunctionT>>(std::move(f)));
				is_small_ = false;
			}
		}

		function(const function &other) noexcept
		{
			if (other.is_small_)
			{
				auto o = reinterpret_cast<const function_holder_base *>(other.aligned_ptr);
				auto p = o->get_aligned_ptr(raw_object);
				aligned_ptr = reinterpret_cast<char *>(p);
				o->small_copy(aligned_ptr);
			}
			else
			{
				aligned_ptr = raw_object;
				f_pointer_ = other.f_pointer_->clone();
			}
			is_small_ = other.is_small_;
		}

		function(function&& other) noexcept
		{
			if (other.is_small_)
			{
				auto o = reinterpret_cast<function_holder_base *>(other.aligned_ptr);
				auto p = o->get_aligned_ptr(raw_object);
				aligned_ptr = reinterpret_cast<char *>(p);
				o->small_move(aligned_ptr);
				o->~function_holder_base();
				new (other.aligned_ptr) std::unique_ptr<function_holder_base>(nullptr);
			}
			else
			{
				aligned_ptr = raw_object;
				new (aligned_ptr) std::unique_ptr<function_holder_base>(std::move(other.f_pointer_));
			}
			is_small_ = other.is_small_;
			other.is_small_ = false;
		}

		function& operator=(const function &other)
		{
			function tmp(other);
			swap(tmp);
			return *this;
		}

		function& operator=(function&& other) noexcept
		{
			destroy();
			if (other.is_small_)
			{
				auto o = reinterpret_cast<function_holder_base *>(other.aligned_ptr);
				o->small_move(aligned_ptr);
				o->~function_holder_base();
				new (other.aligned_ptr) std::unique_ptr<function_holder_base>(nullptr);
			}
			else
			{
				new (aligned_ptr) std::unique_ptr<function_holder_base>(std::move(other.f_pointer_));
			}
			is_small_ = other.is_small_;
			other.is_small_ = false;
			return *this;
		}

		ReturnType operator()(Args&& ...args) const
		{
			if (is_small_)
			{
				return reinterpret_cast<function_holder_base *>(aligned_ptr)->invoke(std::forward<Args>(args)...);
			}
			return f_pointer_->invoke(std::forward<Args>(args)...);
		}

		explicit operator bool() const noexcept
		{
			return is_small_ || f_pointer_ != nullptr;
		}

		~function()
		{
			destroy();
		}

		void swap(function& other) noexcept
		{
			function tmp(std::move(other));
			other = std::move(*this);
			*this = std::move(tmp);
		}

	private:

		struct function_holder_base
		{
			function_holder_base() = default;
			virtual ~function_holder_base() = default;

			virtual ReturnType invoke(Args&& ...args) = 0;
			virtual std::unique_ptr<function_holder_base> clone() const = 0;

			virtual void small_copy(void *) const = 0;
			virtual void small_move(void *) = 0;

			virtual void * get_aligned_ptr(char *) = 0;

			function_holder_base(const function_holder_base &) = delete;
			void operator=(const function_holder_base &) = delete;
		};

		using f_pointer_t = std::unique_ptr<function_holder_base>;

		template <typename FunctionType>
		struct function_holder : public function_holder_base
		{

			function_holder(FunctionType func)
				: function_holder_base(), function_object_(func)
			{}

			ReturnType invoke(Args&& ...args) override
			{
				return function_object_(std::forward<Args>(args)...);
			}

			f_pointer_t clone() const override
			{
				return f_pointer_t(new function_holder<FunctionType>(function_object_));
			}

			void small_copy(void * address) const override
			{
				new (address) function_holder<FunctionType>(function_object_);
			}

			void small_move(void * address) override
			{
				new (address) function_holder<FunctionType>(std::move(function_object_));
			}

			void * get_aligned_ptr(char * address) override
			{
				void * p = reinterpret_cast<void *>(address);
				size_t space = small_object_size;
				return std::align(alignof(function_holder<FunctionType>), sizeof(function_holder<FunctionType>), p, space);
			}

		private:
			FunctionType function_object_;
		};

	private:

		void destroy()
		{
			if (is_small_)
			{
				auto o = get_function_small_object();
				o->~function_holder_base();
			}
			else
			{
				f_pointer_.~unique_ptr();
			}
		}

		decltype(auto) get_function_small_object()
		{
			return reinterpret_cast<function_holder_base *>(aligned_ptr);
		}

		union
		{
			f_pointer_t f_pointer_;
			mutable char raw_object[small_object_size];
		};
		char * aligned_ptr;
		bool is_small_;
	};

}
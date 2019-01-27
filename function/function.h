#pragma once

#define _ENABLE_EXTENDED_ALIGNED_STORAGE

#include <memory>

namespace vsklamm
{
	constexpr size_t small_object_size = 32;
	constexpr size_t small_object_align = 32;

	template <typename Signature>
	struct function;

	template <typename ReturnType, typename ...Args>
	struct function <ReturnType(Args...)>
	{
		using small_object_t = std::aligned_storage_t<small_object_size, small_object_align>;

		function() noexcept : f_pointer_(nullptr), is_small_(false) {}

		function(std::nullptr_t) noexcept : f_pointer_(nullptr), is_small_(false) {}

		template <typename FunctionT>
		function(FunctionT f)
		{
			if (/* check for space */ std::is_nothrow_move_constructible<FunctionT>::value 
				&& sizeof(function_holder<FunctionT>) <= small_object_size)
			{
				new (&small_object) function_holder<FunctionT>(std::move(f));
				is_small_ = true;
			}
			else
			{
				new (&small_object) std::unique_ptr<function_holder_base>(std::make_unique<function_holder<FunctionT>>(std::move(f)));
				is_small_ = false;
			}
		}

		function(const function &other) noexcept
		{
			if (other.is_small_)
			{
				(reinterpret_cast<const function_holder_base *>(&other.small_object))->small_copy(&small_object);
			}
			else
			{
				f_pointer_ = other.f_pointer_->clone();
			}
			is_small_ = other.is_small_;
		}

		function(function&& other) noexcept
		{
			if (other.is_small_)
			{
				auto o = reinterpret_cast<function_holder_base *>(&other.small_object);
				o->small_move(&small_object);
				o->~function_holder_base();
				new (&other.small_object) std::unique_ptr<function_holder_base>(nullptr);
			}
			else
			{
				new (&small_object) std::unique_ptr<function_holder_base>(std::move(other.f_pointer_));
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
				auto o = reinterpret_cast<function_holder_base *>(&other.small_object);
				o->small_move(&small_object);
				o->~function_holder_base();
				new (&other.small_object) std::unique_ptr<function_holder_base>(nullptr);
			}
			else
			{
				new (&small_object) std::unique_ptr<function_holder_base>(std::move(other.f_pointer_));
			}
			is_small_ = other.is_small_;
			other.is_small_ = false;
			return *this;
		}

		ReturnType operator()(Args&& ...args) const
		{
			if (is_small_)
			{
				return reinterpret_cast<function_holder_base *>(&small_object)->invoke(std::forward<Args>(args)...);
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

		void destroy()
		{
			if (is_small_)
			{
				(reinterpret_cast<function_holder_base *>(&small_object))->~function_holder_base();
			}
			else
			{
				f_pointer_.~unique_ptr();
			}
		}

		union
		{
			f_pointer_t f_pointer_;
			mutable small_object_t small_object;			
		};

		bool is_small_;
	};

}

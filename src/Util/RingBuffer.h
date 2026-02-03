#pragma once

namespace Util
{
	template <typename T, size_t N>
	struct RingBuffer
	{
	  public:
		RingBuffer() = default;
		template <typename... Args>
		RingBuffer(Args&&... args) : _buffer{ std::forward<Args>(args)... }, _head{ sizeof...(Args) % N }, _size{ sizeof...(Args) }
		{}
		~RingBuffer() = default;

	  public:
		void push(const T& value)
		{
			_buffer[_head] = value;
			_head = (_head + 1) % N;
			if (_size < N) {
				_size++;
			}
		}

		const std::array<const T, N>& view() const
		{
			return _buffer;
		}

		template <typename K>
		K to() const
		{
			K result;
			if constexpr (requires { result[0]; result.size(); }) {
				if constexpr (result.size() != N) {
					static_assert(sizeof(K) == 0, "Type K must have size() equal to N");
				}
				for (size_t i = 0; i < _size; ++i) {
					result[i] = _buffer[(_head + i) % N];
				}
			} else if constexpr (requires { result.push_back(std::declval<T>()); }) {
				for (size_t i = 0; i < _size; ++i) {
					result.push_back(_buffer[(_head + i) % N]);
				}
			} else {
				static_assert(sizeof(K) == 0, "Type K must support either operator[] with size() or push_back()");
			}
			return result;
		}

	  public:
		size_t size() const { return _size; }
		size_t length() const { return _size; }

		size_t capacity() const { return N; }

	  public:
		const T& operator[](size_t index) const { return _buffer[(_head + index) % N]; }
		T& operator[](size_t index) { return _buffer[(_head + index) % N]; }

	  private:
		std::array<T, N> _buffer{};
		size_t _head{ 0 };
		size_t _size{ 0 };
	};

}  // namespace Util

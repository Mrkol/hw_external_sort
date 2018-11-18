#include <string>
#include <fstream>
#include <iostream>
#include <cmath>
#include <vector>
#include <iterator>
#include <cstdio>
#include <algorithm>

// Interface

template<typename T>
void serialize(T value, std::ostream& out);

template<typename T>
T deserialize(std::istream& in);

template<typename T>
class SerializeIterator
{
 public:
	typedef void value_type;
	typedef void difference_type;
	typedef void pointer;
	typedef void reference;
	typedef std::output_iterator_tag iterator_category;

	explicit SerializeIterator(std::ostream& stream);

	SerializeIterator& operator=(const T& value);
	SerializeIterator& operator*() { return *this; } // does nothing
	SerializeIterator& operator++() { return *this;} // does nothing
	SerializeIterator& operator++(int) { return *this; } // does nothing

 private:
	std::ostream* stream_;
};

template<typename T>
class DeserializeIterator 
{
 public:
	typedef T value_type;
	typedef std::ptrdiff_t difference_type;
	typedef T* pointer;
	typedef T& reference;
	typedef std::input_iterator_tag iterator_category;

	DeserializeIterator();
	explicit DeserializeIterator(std::istream& stream);

	const T& operator*() const;
	const T& operator->() const;
	
    DeserializeIterator& operator++();
	DeserializeIterator operator++(int);

	bool isEnd() const;

 private:
	std::istream* stream_;
	T value_;
	bool isEnd_;
};

template<typename T>
bool operator==(const DeserializeIterator<T>& first, const DeserializeIterator<T>& second);

template<typename T>
bool operator!=(const DeserializeIterator<T>& first, const DeserializeIterator<T>& second);

std::string tempFilename();

template<typename InputIter, typename OutputIter, typename Merger>
class ExternalAlgoritm 
{
 public:
	typedef typename std::iterator_traits<InputIter>::value_type value_type;

	ExternalAlgoritm(InputIter begin, InputIter end,
									 size_t size, size_t maxObjectsInMemory,
									 OutputIter out);

	~ExternalAlgoritm();

	void run();

 private:
	virtual void prepare(std::vector<value_type>& data) = 0;

	InputIter begin_;
	InputIter end_;
	size_t size_;
	size_t maxObjectsInMemory_;
	OutputIter out_;

	size_t countOfFiles_;
	std::fstream* fstreams_;
	std::vector<std::string> filenames_;
};

template<class T>
struct DeserializerCompare
{
	bool operator()(const DeserializeIterator<T>& first, const DeserializeIterator<T>& second);
};

template<class T>
class SortMerger
{
 public:
	explicit SortMerger(const std::vector<DeserializeIterator<T>>& deserializers);

	bool hasNext() const;
	T next();

 private:
	std::vector<DeserializeIterator<T>> deserializers_;
};

template<typename InputIter, typename OutputIter>
class ExternalSort : public ExternalAlgoritm<
	InputIter, OutputIter, SortMerger<typename std::iterator_traits<InputIter>::value_type>> 
{
 public:
	typedef ExternalAlgoritm<
		InputIter, OutputIter, SortMerger<typename std::iterator_traits<InputIter>::value_type>> Base;

		ExternalSort(InputIter begin, InputIter end,
										 size_t size, size_t maxObjectsInMemory,
										 OutputIter out);

 private:
	virtual void prepare(std::vector<typename Base::value_type>& container);
};

template<class T>
class ReverseMerger 
{
 public:
	explicit ReverseMerger(const std::vector<DeserializeIterator<T>>& deserializers);

	bool hasNext() const;
	T next();

 private:
	std::vector<DeserializeIterator<T>> deserializers_;
};

template<typename InputIter, typename OutputIter>
class ExternalReverse : public ExternalAlgoritm<
	InputIter, OutputIter, ReverseMerger<typename std::iterator_traits<InputIter>::value_type>> 
{
 public:
	typedef ExternalAlgoritm<
		InputIter, OutputIter, ReverseMerger<typename std::iterator_traits<InputIter>::value_type>> Base;

		ExternalReverse(InputIter begin, InputIter end,
										 size_t size, size_t maxObjectsInMemory,
										 OutputIter out);

 private:
	virtual void prepare(std::vector<typename Base::value_type>& container);
};

// Implementation

template<typename T>
void serialize(T value, std::ostream& out)
{
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
T deserialize(std::istream& in)
{
    T result;
    in.read(reinterpret_cast<char*>(&result), sizeof(T));
    return result;
}

template<typename T>
SerializeIterator<T>::SerializeIterator(std::ostream& stream) 
    : stream_(&stream)
{

}

template<typename T>
SerializeIterator<T>& SerializeIterator<T>::operator=(const T& value)
{
    serialize(value, *stream_);
    return *this;
}

template<typename T>
DeserializeIterator<T>::DeserializeIterator()
    : isEnd_(true)
{

}

template<typename T>
DeserializeIterator<T>::DeserializeIterator(std::istream& stream)
    : stream_(&stream), isEnd_(false)
{
    operator++();
}

template<typename T>
bool DeserializeIterator<T>::isEnd() const
{
    return isEnd_;
}

template<typename T>
const T& DeserializeIterator<T>::operator*() const
{
    return value_;
}

template<typename T>
const T& DeserializeIterator<T>::operator->() const
{
    return value_;
}

template<typename T>
DeserializeIterator<T>& DeserializeIterator<T>::operator++()
{
    if (isEnd_)
    {
        std::cerr << "Error: tried to advance a deserialization";
        std::cerr << " iterator which has already reached it's end." << std::endl;
        throw 1;
    }

    value_ = deserialize<T>(*stream_);

    if (stream_->fail())
    {
        isEnd_ = true;
    }
    return *this;
}

template<typename T>
DeserializeIterator<T> DeserializeIterator<T>::operator++(int)
{
    DeserializeIterator<T> copy = *this;
    ++*this;
    return copy;
}

template<typename T>
bool operator==(const DeserializeIterator<T>& first, const DeserializeIterator<T>& second)
{
    return first.stream_ == second.stream_ || (first.isEnd() && second.isEnd());
}

template<typename T>
bool operator!=(const DeserializeIterator<T>& first, const DeserializeIterator<T>& second)
{
    return !(first == second);
}

std::string tempFilename()
{
    static int filenumber = 0;
    return "file" + std::to_string(filenumber++) + ".tmp";
}

template<typename InputIter, typename OutputIter, typename Merger>
ExternalAlgoritm<InputIter, OutputIter, Merger>::ExternalAlgoritm(InputIter begin, 
    InputIter end, size_t size, size_t maxObjectsInMemory, OutputIter out)
    : begin_(begin), end_(end), size_(size), maxObjectsInMemory_(maxObjectsInMemory), out_(out),
        countOfFiles_(0), fstreams_(nullptr)
{

}

template<typename InputIter, typename OutputIter, typename Merger>
ExternalAlgoritm<InputIter, OutputIter, Merger>::~ExternalAlgoritm()
{
    delete[] fstreams_;
}

template<typename InputIter, typename OutputIter, typename Merger>
void ExternalAlgoritm<InputIter, OutputIter, Merger>::run()
{
    //fun goes here

    while (begin_ != end_)
    {
        std::vector<value_type> chunk;
        chunk.reserve(maxObjectsInMemory_);

        while (begin_ != end_ && chunk.size() < maxObjectsInMemory_)
        {
            chunk.push_back(*begin_);
            ++begin_;
        }

        prepare(chunk);

        std::string file = tempFilename();

        std::ofstream stream(file);
        SerializeIterator<value_type> writer(stream);
        std::copy(chunk.begin(), chunk.end(), writer);
        stream.close();


        countOfFiles_++;
        filenames_.push_back(file);
    }

    //k-way merge

    if (fstreams_ != nullptr) delete[] fstreams_;
    fstreams_ = new std::fstream[countOfFiles_];
    std::vector<DeserializeIterator<value_type>> deserializers(countOfFiles_);
    for (size_t i = 0; i < countOfFiles_; i++)
    {
        fstreams_[i].open(filenames_[i], std::fstream::in);
        deserializers[i] = DeserializeIterator<value_type>(fstreams_[i]);
    }

    Merger merger(deserializers);

    while (merger.hasNext())
    {
        out_ = merger.next();
        out_++;
    }

    for (size_t i = 0; i < countOfFiles_; i++)
    {
        fstreams_[i].close();
    }
}

template<typename T>
bool DeserializerCompare<T>::operator()(
    const DeserializeIterator<T>& first, const DeserializeIterator<T>& second)
{
    return *first > *second;
}

template<typename T>
SortMerger<T>::SortMerger(const std::vector<DeserializeIterator<T>>& deserializers)
    : deserializers_(deserializers)
{
    std::make_heap(deserializers_.begin(), deserializers_.end(), 
        DeserializerCompare<T>());
}

template<typename T>
bool SortMerger<T>::hasNext() const
{
    return !deserializers_.empty();
}

template<typename T>
T SortMerger<T>::next()
{
    if (!hasNext()) 
    {
        std::cerr << "Error: sort merger ran out of elements to merge." << std::endl;
        throw 1;
    }

    std::pop_heap(deserializers_.begin(), deserializers_.end(), DeserializerCompare<T>());

    T result = *deserializers_.back();
    ++deserializers_.back();

    if (deserializers_.back().isEnd()) deserializers_.pop_back();
    else std::push_heap(deserializers_.begin(), deserializers_.end(), DeserializerCompare<T>());

    return result;
}

template<typename InputIter, typename OutputIter>
ExternalSort<InputIter, OutputIter>::ExternalSort(
    InputIter begin, InputIter end, size_t size, 
    size_t maxObjectsInMemory, OutputIter out)
    : Base(begin, end, size, maxObjectsInMemory, out)
{

}

template<typename InputIter, typename OutputIter>
void ExternalSort<InputIter, OutputIter>::prepare(
    std::vector<typename Base::value_type>& container)
{
    std::sort(container.begin(), container.end());
}

template<typename T>
ReverseMerger<T>::ReverseMerger(const std::vector<DeserializeIterator<T>>& deserializers)
    : deserializers_(deserializers)
{

}

template<typename T>
bool ReverseMerger<T>::hasNext() const
{
    return !deserializers_.empty();
}

template<typename T>
T ReverseMerger<T>::next()
{
    if (!hasNext()) 
    {
        std::cerr << "Error: reverse merger ran out of elements to merge." << std::endl;
        throw 1;
    }

    T result = *deserializers_.back();
    ++deserializers_.back();

    if (deserializers_.back().isEnd())
        deserializers_.pop_back();

    return result;
}

template<typename InputIter, typename OutputIter>
ExternalReverse<InputIter, OutputIter>::ExternalReverse(
    InputIter begin, InputIter end, size_t size, 
    size_t maxObjectsInMemory, OutputIter out)
    : Base(begin, end, size, maxObjectsInMemory, out)
{
    
}

template<typename InputIter, typename OutputIter>
void ExternalReverse<InputIter, OutputIter>::prepare(
    std::vector<typename Base::value_type>& container)
{
    std::reverse(container.begin(), container.end());
}

int main()
{
	std::ifstream ifs("input.txt");
	std::ofstream ofs("output.txt");
	size_t type, count, max;
	ifs >> type >> count >> max;

	if (type == 1) {
		ExternalSort<
			std::istream_iterator<int>,
			std::ostream_iterator<int>
		> alg(
			std::istream_iterator<int>(ifs), std::istream_iterator<int>(),
			count, max,
			std::ostream_iterator<int>(ofs, " "));
		alg.run();
	}
    else 
    {
		ExternalReverse<
			std::istream_iterator<int>,
			std::ostream_iterator<int>
		> alg(
			std::istream_iterator<int>(ifs), std::istream_iterator<int>(),
			count, max,
			std::ostream_iterator<int>(ofs, " "));
		alg.run();
	}
	return 0;
}

#pragma once


template <typename T>
class WeightedAVG {
public:
    explicit WeightedAVG(int16_t factor)
      : _countMax(factor)
      , _count(0), _countNum(0), _avgV(0), _minV(0), _maxV(0), _lastV(0)  {}

    void addNumber(const T& num) {
      if (_count == 0){
          _count++;
          _avgV = num;
          _minV = num;
          _maxV = num;
          _countNum = 1;
      } else {
          if (_count < _countMax) { _count++; }
          _avgV = (_avgV * (_count - 1) + num) / _count;
          if (num < _minV) { _minV = num; }
          if (num > _maxV) { _maxV = num; }
          _countNum++;
      }
      _lastV = num;
    }

    void reset(void) { _count = 0; _avgV = 0; _minV = 0; _maxV = 0; _lastV = 0; _countNum = 0; }
    void reset(const T& num) { _count = 0; addNumber(num); }
    T getAverage() const { return _avgV; }
    T getMin() const { return _minV; }
    T getMax() const { return _maxV; }
    T getLast() const { return _lastV; }
    int32_t getCounts() const { return _countNum; }

private:
    int16_t _countMax;    // weighting factor (10 => 1/10 => 10%)
    int16_t _count;       // counter (0 - _countMax)
    int32_t _countNum;    // counts the amount of added numbers
    T _avgV;               // average value
    T _minV;               // minimum value
    T _maxV;               // maximum value
    T _lastV;              // last value
};


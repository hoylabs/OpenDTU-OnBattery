// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/*
 * Weighted average and statistics template class
 *
 * Functions: Weighted average, Maximum, Minimum, Last value, Counts the added values and Reset
 * Note: Use the constructor to configure the weighted average, weighted average = 100% / factor,
 * Example: WeightedAVG<uint16_t> myData {20};  weighted average = 5% (100% / 20)
 *
 * 18.05.2025 - 1.10 - improvement: use always float for average value to avoid truncating errors
*/

template <typename T>
class WeightedAVG {
public:
    // Constructor with a factor to calculate the weighted average
    // Example: WeightedAVG<uint16_t> myData {5};  weighted average = 20% (100% / 5)
    explicit WeightedAVG(size_t factor)
      : _countMax(factor)
      , _count(0), _countNum(0), _avgV(0), _minV(0), _maxV(0), _lastV(0)  {}

    // Add a value to the statistics
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
          if (_countNum < 10000) { _countNum++; }
      }
      _lastV = num;
    }

    // Reset the statistic data
    void reset(void) { _count = 0; _avgV = 0.0f; _minV = 0; _maxV = 0; _lastV = 0; _countNum = 0; }
    // Reset the statistic data and initialize with first value
    void reset(const T& num) { _count = 0; addNumber(num); }
    // Returns the weighted average
    T getAverage() const { return static_cast<T>(_avgV); }
    // Returns the minimum value
    T getMin() const { return _minV; }
    // Returns the maximum value
    T getMax() const { return _maxV; }
    // Returns the last added value
    T getLast() const { return _lastV; }
    // Returns the amount of added values. Limited to 10000
    size_t getCounts() const { return _countNum; }

private:
    size_t _countMax;      // weighting factor (10 => 1/10 => 10%)
    size_t _count;         // counter (0 - _countMax)
    size_t _countNum;      // counts the amount of added values (0 - 10000)
    float _avgV;           // average value, always float to avoid truncating errors
    T _minV;               // minimum value
    T _maxV;               // maximum value
    T _lastV;              // last value
};


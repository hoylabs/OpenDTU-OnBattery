// SPDX-License-Identifier: GPL-2.0-or-later

#include <gridcharger/HTTP/DataPoints.h>

template class DataPointContainer<DataPoint<float, std::string>,
                                  GridChargers::HTTP::DataPointLabel,
                                  GridChargers::HTTP::DataPointLabelTraits>;

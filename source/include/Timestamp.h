/*
    Timestamp.h - contains class to generate a human-readable timestamp
    Copyright 2026 Jedidiah Thompson

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef TIMESTAMP_H
#define TIMESTAMP_H

class Timestamp
{
  public:
    static Timestamp MakeTimestampNow()
    {
        Timestamp time;
        time.timePoint = std::chrono::system_clock::now();
        auto baseTime = std::chrono::system_clock::to_time_t (time.timePoint);
        std::tm localTime;
        if (!localtime_r (&baseTime, &localTime))
            throw std::runtime_error ("time conversion failed");

        char buf[64];
        std::strftime (buf, 64, "%Y%m%dT%H%M%S.", &localTime);

        // Add microseconds to it
        auto microseconds =
            std::chrono::duration_cast<std::chrono::microseconds> (time.timePoint.time_since_epoch())
                .count() %
            1000000;

        auto fraction = microseconds / 100;
        std::string fractionString = std::to_string (fraction);
        time.timeStr = std::string (buf) + std::string (4 - fractionString.length(), '0') + fractionString;

        return time;
    }

    explicit operator std::string() const
    {
        return timeStr;
    }
    explicit operator std::chrono::system_clock::time_point() const
    {
        return timePoint;
    }
    explicit operator std::time_t() const
    {
        return std::chrono::system_clock::to_time_t (timePoint);
    }

  private:
    Timestamp() = default;
    std::string timeStr;
    std::chrono::system_clock::time_point timePoint;
};

#endif

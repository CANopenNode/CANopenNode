#pragma once

template <typename T>
static T invertValue(T val) {
    return ~val;
}

// Specialization for bool
template <>
bool invertValue(bool val) {
    return !val;
}
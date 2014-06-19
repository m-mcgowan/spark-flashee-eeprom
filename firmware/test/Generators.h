/* 
 * File:   Generators.h
 * Author: mat
 *
 * Created on 19 June 2014, 18:54
 */

#ifndef GENERATORS_H
#define	GENERATORS_H

#include <vector>

class Generator {
public:
    virtual uint8_t next()=0;    
    
    void skip(unsigned int count) {
        while (count-->0) 
            next();
    }
};


class ValueGenerator : public Generator {
protected:    
    uint8_t _value;
public:
    ValueGenerator(uint8_t value) : _value(value) {}
    
    uint8_t next() { return _value; }
};


class SequenceGenerator : public ValueGenerator {
public:    
    SequenceGenerator(uint8_t startValue) : ValueGenerator(startValue) {}
    uint8_t next() { return _value++; }
};


#ifdef __GXX_EXPERIMENTAL_CXX0X__

#include <random>
class RandomGenerator : public Generator {
    std::default_random_engine generator;
    std::uniform_int_distribution<uint8_t> distribution;
public:
    RandomGenerator(int seed) : generator(seed), distribution(0,255) {}    
    uint8_t next() { return distribution(generator); }
};
#endif


class PushBackGenerator : public Generator {
    
    Generator& delegate;
    std::vector<uint8_t> pushback;
    
public:

    PushBackGenerator(Generator& source) : delegate(source) {}
    
    uint8_t next() {
        if (pushback.empty()) {
            return delegate.next();
        }
        else {
            uint8_t result = pushback.back();
            pushback.pop_back();
            return result;
        }
    }
    
    void push_back(uint8_t value) {
        pushback.push_back(value);
    }
};


#endif	/* GENERATORS_H */


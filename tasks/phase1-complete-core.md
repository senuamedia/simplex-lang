# Phase 1: Complete the Core

**Priority**: CRITICAL - Blocks all other development
**Estimated Effort**: 2-3 weeks
**Status**: Not Started

## Overview

The core language primitives must be completed before building libraries or ecosystem. Everything else depends on these fundamentals.

---

## 1. HashMap Implementation

**File**: `standalone_runtime.c` + `simplex-std/src/collections/map.sx`
**Status**: Declared but not implemented

### Subtasks

- [ ] 1.1 Implement `HashMap` struct in C runtime
  - Hash table with open addressing or chaining
  - Initial capacity and load factor handling
  - Automatic resize/rehash

- [ ] 1.2 Core operations
  - [ ] `hashmap_new()` - Create empty map
  - [ ] `hashmap_with_capacity(n)` - Create with initial capacity
  - [ ] `hashmap_insert(map, key, value)` - Insert key-value pair
  - [ ] `hashmap_get(map, key)` - Get value by key (returns Option)
  - [ ] `hashmap_remove(map, key)` - Remove and return value
  - [ ] `hashmap_contains(map, key)` - Check if key exists
  - [ ] `hashmap_len(map)` - Get number of entries
  - [ ] `hashmap_is_empty(map)` - Check if empty
  - [ ] `hashmap_clear(map)` - Remove all entries
  - [ ] `hashmap_free(map)` - Deallocate map

- [ ] 1.3 Iteration support
  - [ ] `hashmap_keys(map)` - Return iterator over keys
  - [ ] `hashmap_values(map)` - Return iterator over values
  - [ ] `hashmap_iter(map)` - Return iterator over (key, value) pairs

- [ ] 1.4 Simplex wrapper in `map.sx`
  - [ ] Generic `HashMap<K, V>` type definition
  - [ ] Method syntax: `map.insert(k, v)`, `map.get(k)`
  - [ ] Integration with `Hash` trait

- [ ] 1.5 Tests
  - [ ] Basic insert/get/remove
  - [ ] Collision handling
  - [ ] Resize behavior
  - [ ] Iteration order (unordered)
  - [ ] Memory leak tests

---

## 2. HashSet Implementation

**File**: `standalone_runtime.c` + `simplex-std/src/collections/set.sx`
**Status**: Declared but not implemented

### Subtasks

- [ ] 2.1 Implement `HashSet` struct (wrapper around HashMap with unit values)

- [ ] 2.2 Core operations
  - [ ] `hashset_new()` - Create empty set
  - [ ] `hashset_insert(set, value)` - Add value
  - [ ] `hashset_remove(set, value)` - Remove value
  - [ ] `hashset_contains(set, value)` - Check membership
  - [ ] `hashset_len(set)` - Get size
  - [ ] `hashset_is_empty(set)` - Check if empty
  - [ ] `hashset_clear(set)` - Remove all
  - [ ] `hashset_free(set)` - Deallocate

- [ ] 2.3 Set operations
  - [ ] `hashset_union(a, b)` - Union of two sets
  - [ ] `hashset_intersection(a, b)` - Intersection
  - [ ] `hashset_difference(a, b)` - Difference (a - b)
  - [ ] `hashset_symmetric_difference(a, b)` - XOR
  - [ ] `hashset_is_subset(a, b)` - Check subset
  - [ ] `hashset_is_superset(a, b)` - Check superset

- [ ] 2.4 Iteration support
  - [ ] `hashset_iter(set)` - Return iterator over values

- [ ] 2.5 Simplex wrapper in `set.sx`
  - [ ] Generic `HashSet<T>` type definition
  - [ ] Method syntax

- [ ] 2.6 Tests
  - [ ] Basic operations
  - [ ] Set algebra operations
  - [ ] Edge cases (empty sets, self-operations)

---

## 3. String Operations

**File**: `standalone_runtime.c`
**Status**: Basic operations exist, many missing

### Subtasks

- [ ] 3.1 Search operations
  - [ ] `string_contains(s, substr)` - Check if contains substring
  - [ ] `string_starts_with(s, prefix)` - Check prefix
  - [ ] `string_ends_with(s, suffix)` - Check suffix
  - [ ] `string_find(s, substr)` - Find first index of substring (-1 if not found)
  - [ ] `string_rfind(s, substr)` - Find last index of substring
  - [ ] `string_count(s, substr)` - Count occurrences

- [ ] 3.2 Split and join
  - [ ] `string_split(s, delim)` - Split by delimiter, return Vec<String>
  - [ ] `string_split_n(s, delim, n)` - Split into at most n parts
  - [ ] `string_split_whitespace(s)` - Split by whitespace
  - [ ] `string_lines(s)` - Split by newlines
  - [ ] `string_join(vec, sep)` - Join strings with separator

- [ ] 3.3 Transformation
  - [ ] `string_replace(s, from, to)` - Replace all occurrences
  - [ ] `string_replace_n(s, from, to, n)` - Replace first n occurrences
  - [ ] `string_to_lowercase(s)` - Convert to lowercase
  - [ ] `string_to_uppercase(s)` - Convert to uppercase
  - [ ] `string_reverse(s)` - Reverse string

- [ ] 3.4 Trimming (extend existing)
  - [ ] `string_trim_start(s)` - Trim leading whitespace
  - [ ] `string_trim_end(s)` - Trim trailing whitespace
  - [ ] `string_trim_chars(s, chars)` - Trim specific characters

- [ ] 3.5 Padding
  - [ ] `string_pad_left(s, len, char)` - Left pad to length
  - [ ] `string_pad_right(s, len, char)` - Right pad to length
  - [ ] `string_center(s, len, char)` - Center with padding

- [ ] 3.6 Parsing
  - [ ] `string_parse_i64(s)` - Parse as i64 (Result)
  - [ ] `string_parse_f64(s)` - Parse as f64 (Result)
  - [ ] `string_parse_bool(s)` - Parse as bool (Result)

- [ ] 3.7 Character operations
  - [ ] `string_chars(s)` - Return iterator over characters
  - [ ] `string_bytes(s)` - Return iterator over bytes
  - [ ] `char_is_digit(c)` - Check if digit
  - [ ] `char_is_alpha(c)` - Check if letter
  - [ ] `char_is_alphanumeric(c)` - Check if alphanumeric
  - [ ] `char_is_whitespace(c)` - Check if whitespace

- [ ] 3.8 Tests
  - [ ] All operations with edge cases
  - [ ] Unicode handling (if applicable)
  - [ ] Empty string handling

---

## 4. Iterator Combinators

**File**: `standalone_runtime.c` + `simplex-std/src/iter.sx`
**Status**: Basic `iter_next` exists, combinators missing

### Subtasks

- [ ] 4.1 Core iterator trait
  - [ ] Define `Iterator` trait with `next()` method
  - [ ] `iter_new(collection)` - Create iterator from collection

- [ ] 4.2 Transformation combinators
  - [ ] `iter_map(iter, fn)` - Transform each element
  - [ ] `iter_filter(iter, predicate)` - Keep elements matching predicate
  - [ ] `iter_filter_map(iter, fn)` - Filter and map combined
  - [ ] `iter_flat_map(iter, fn)` - Map and flatten
  - [ ] `iter_enumerate(iter)` - Add index to each element
  - [ ] `iter_skip(iter, n)` - Skip first n elements
  - [ ] `iter_take(iter, n)` - Take first n elements
  - [ ] `iter_step_by(iter, n)` - Take every nth element
  - [ ] `iter_chain(iter1, iter2)` - Chain two iterators
  - [ ] `iter_zip(iter1, iter2)` - Zip two iterators
  - [ ] `iter_rev(iter)` - Reverse iterator (if reversible)

- [ ] 4.3 Consuming combinators
  - [ ] `iter_collect_vec(iter)` - Collect into Vec
  - [ ] `iter_collect_hashmap(iter)` - Collect pairs into HashMap
  - [ ] `iter_collect_hashset(iter)` - Collect into HashSet
  - [ ] `iter_fold(iter, init, fn)` - Fold/reduce with accumulator
  - [ ] `iter_reduce(iter, fn)` - Reduce without initial value
  - [ ] `iter_sum(iter)` - Sum all elements
  - [ ] `iter_product(iter)` - Product of all elements
  - [ ] `iter_count(iter)` - Count elements
  - [ ] `iter_for_each(iter, fn)` - Execute function for each

- [ ] 4.4 Search combinators
  - [ ] `iter_find(iter, predicate)` - Find first matching
  - [ ] `iter_position(iter, predicate)` - Find index of first matching
  - [ ] `iter_any(iter, predicate)` - Check if any match
  - [ ] `iter_all(iter, predicate)` - Check if all match
  - [ ] `iter_min(iter)` - Find minimum
  - [ ] `iter_max(iter)` - Find maximum
  - [ ] `iter_min_by(iter, fn)` - Find minimum by key
  - [ ] `iter_max_by(iter, fn)` - Find maximum by key

- [ ] 4.5 Simplex syntax integration
  - [ ] Method chaining: `vec.iter().map(f).filter(p).collect()`
  - [ ] For-loop integration: `for x in iter { }`

- [ ] 4.6 Tests
  - [ ] Each combinator individually
  - [ ] Chained combinations
  - [ ] Lazy evaluation verification
  - [ ] Empty iterator handling

---

## 5. Result/Option Error Handling Chain

**File**: `simplex-std/src/result.sx` + codegen support
**Status**: Basic enum exists, methods missing

### Subtasks

- [ ] 5.1 Option<T> methods
  - [ ] `is_some()` - Check if Some
  - [ ] `is_none()` - Check if None
  - [ ] `unwrap()` - Get value or panic
  - [ ] `unwrap_or(default)` - Get value or default
  - [ ] `unwrap_or_else(fn)` - Get value or compute default
  - [ ] `expect(msg)` - Get value or panic with message
  - [ ] `map(fn)` - Transform inner value
  - [ ] `map_or(default, fn)` - Map or return default
  - [ ] `map_or_else(default_fn, fn)` - Map or compute default
  - [ ] `and(other)` - Return other if Some, else None
  - [ ] `and_then(fn)` - Flatmap
  - [ ] `or(other)` - Return self if Some, else other
  - [ ] `or_else(fn)` - Return self if Some, else compute
  - [ ] `filter(predicate)` - Keep if predicate true
  - [ ] `ok_or(err)` - Convert to Result
  - [ ] `ok_or_else(fn)` - Convert to Result with computed error

- [ ] 5.2 Result<T, E> methods
  - [ ] `is_ok()` - Check if Ok
  - [ ] `is_err()` - Check if Err
  - [ ] `ok()` - Convert to Option<T>
  - [ ] `err()` - Convert to Option<E>
  - [ ] `unwrap()` - Get value or panic
  - [ ] `unwrap_err()` - Get error or panic
  - [ ] `unwrap_or(default)` - Get value or default
  - [ ] `unwrap_or_else(fn)` - Get value or compute from error
  - [ ] `expect(msg)` - Get value or panic with message
  - [ ] `expect_err(msg)` - Get error or panic with message
  - [ ] `map(fn)` - Transform Ok value
  - [ ] `map_err(fn)` - Transform Err value
  - [ ] `and(other)` - Return other if Ok, else Err
  - [ ] `and_then(fn)` - Flatmap Ok
  - [ ] `or(other)` - Return self if Ok, else other
  - [ ] `or_else(fn)` - Return self if Ok, else compute

- [ ] 5.3 `?` operator support
  - [ ] Parser support for `?` postfix operator
  - [ ] Codegen for early return on Err/None
  - [ ] Proper error propagation

- [ ] 5.4 Tests
  - [ ] All Option methods
  - [ ] All Result methods
  - [ ] `?` operator chaining
  - [ ] Error propagation across function boundaries

---

## 6. JSON Serialization (Complete)

**File**: `standalone_runtime.c` + `simplex-std/src/json.sx`
**Status**: Parsing exists, serialization incomplete

### Subtasks

- [ ] 6.1 JSON Value type (verify/complete)
  - [ ] `JsonValue` enum: Null, Bool, Number, String, Array, Object
  - [ ] Constructors for each variant

- [ ] 6.2 JSON Parsing (verify working)
  - [ ] `json_parse(s)` - Parse string to JsonValue
  - [ ] Proper error handling for malformed JSON
  - [ ] Unicode escape handling
  - [ ] Number parsing (integers and floats)

- [ ] 6.3 JSON Serialization (implement)
  - [ ] `json_stringify(value)` - Convert JsonValue to string
  - [ ] `json_stringify_pretty(value, indent)` - Pretty print
  - [ ] Proper escaping of special characters
  - [ ] Unicode output

- [ ] 6.4 Value accessors
  - [ ] `json_get(value, key)` - Get object field
  - [ ] `json_get_index(value, i)` - Get array element
  - [ ] `json_as_bool(value)` - Convert to bool
  - [ ] `json_as_i64(value)` - Convert to i64
  - [ ] `json_as_f64(value)` - Convert to f64
  - [ ] `json_as_string(value)` - Convert to String
  - [ ] `json_as_array(value)` - Get as array
  - [ ] `json_as_object(value)` - Get as object
  - [ ] `json_is_null/bool/number/string/array/object(value)` - Type checks

- [ ] 6.5 Value builders
  - [ ] `json_null()` - Create null
  - [ ] `json_bool(b)` - Create bool
  - [ ] `json_number(n)` - Create number
  - [ ] `json_string(s)` - Create string
  - [ ] `json_array()` - Create empty array
  - [ ] `json_object()` - Create empty object
  - [ ] `json_array_push(arr, val)` - Add to array
  - [ ] `json_object_set(obj, key, val)` - Set object field

- [ ] 6.6 Derive macro (future)
  - [ ] `#[derive(JsonSerialize)]` for structs
  - [ ] `#[derive(JsonDeserialize)]` for structs

- [ ] 6.7 Tests
  - [ ] Parse/stringify round-trip
  - [ ] Complex nested structures
  - [ ] Edge cases (empty, deeply nested)
  - [ ] Error handling

---

## Completion Criteria

Phase 1 is complete when:
- [ ] HashMap and HashSet are fully implemented with tests passing
- [ ] All string operations are available and tested
- [ ] Iterator combinators work with method chaining
- [ ] Option/Result have full method suites
- [ ] JSON can be parsed AND serialized
- [ ] All new features have documentation comments

---

## Dependencies

- None (this is the foundation)

## Dependents

- Phase 2 (Package Ecosystem) - needs JSON for manifest files
- Phase 3 (Essential Libraries) - needs HashMap, iterators
- Phase 4 (AI/Actor) - needs all core features

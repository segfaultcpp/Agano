#include <iostream>
#include <type_traits>
#include <vector>
#include <thread>
#include <ranges>
#include <format>
#include <cassert>
#include "agano.hpp"

void test1() noexcept {
    auto counter1 = agano::make_thread_bound<int>(42);

    std::thread t{
        [&] {
            auto counter2 = counter1;
            assert(counter1.get_owner_id() != counter2.get_owner_id());

            auto counter3 = std::move(counter1);
            // here use-after-move occurred. Remove bugprone-use-after-move from WarningsAsErrors section
            // in .clang-tidy file for test purposes
            assert(counter1.is_unbound()); 

            assert(counter2.get_owner_id() == counter3.get_owner_id());
        }
    };

    t.join();
}

void test2() noexcept {
    auto counter = agano::make_thread_bound<int>(agano::defer_binding, 42);
    std::thread t{
        [counter1 = std::move(counter)]() mutable {
            auto counter2 = counter1;
            assert(counter1.get_owner_id() == counter2.get_owner_id());
            assert(counter1.is_unbound());

            *counter1 += 50;
            assert(counter1.get_owner_id() == std::this_thread::get_id());
            
            assert(counter2.is_unbound());
            assert(*counter2 == 42);
            assert(!counter2.is_unbound());

            auto counter3 = std::move(counter1);
            assert(counter1.is_unbound());

            assert(counter2.get_owner_id() == counter3.get_owner_id());
        }
    };

    t.join();
}

void intentional_error() noexcept {
    auto counter = agano::make_thread_bound<int>(0);
    *counter += 10;

    std::thread t{
        [&] {
            std::cout << *counter << "\n";
        }
    };
    *counter += 10;
    std::cout << *counter << "\n";
    t.join();
}

int main () {
    test1();
    test2();
    intentional_error();
    return 0;
}
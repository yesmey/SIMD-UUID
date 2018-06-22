#include <benchmark/benchmark.h>
#include "UUID.h"
#include <Windows.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_io.hpp>

#define BOOST_UUID_USE_SSE41 1

static void BM_MyUUID(benchmark::State& state) {
	std::string x = "256a4180-65aa-42ec-a945-5fd21dec0538";
	for (auto _ : state)
	{
		meyr::UUID guid;
		guid.parse(x);
	}
}
BENCHMARK(BM_MyUUID);

static void BM_WindowsUUID(benchmark::State& state) {
	auto x = L"{256a4180-65aa-42ec-a945-5fd21dec0538}";
	for (auto _ : state)
	{
		GUID guid;
		CLSIDFromString(x, (LPCLSID)&guid);
	}
}
BENCHMARK(BM_WindowsUUID);

static void BM_BoostUUID(benchmark::State& state) {
	std::string x = "{256a4180-65aa-42ec-a945-5fd21dec0538}";
	for (auto _ : state)
	{
		boost::uuids::string_generator gen;
		boost::uuids::uuid u1 = gen(x);
	}
}
BENCHMARK(BM_BoostUUID);

static void BM_MyUUIDToString(benchmark::State& state) {
	std::string x = "256a4180-65aa-42ec-a945-5fd21dec0538";
	meyr::UUID guid;
	guid.parse(x);

	for (auto _ : state)
	{
		std::string s1 = guid.to_string('D');
	}
}
BENCHMARK(BM_MyUUIDToString);


static void BM_BoostUUIDToString(benchmark::State& state) {
	std::string x = "{256a4180-65aa-42ec-a945-5fd21dec0538}";
	boost::uuids::string_generator gen;
	boost::uuids::uuid u1 = gen(x);

	for (auto _ : state)
	{
		std::string s1 = boost::uuids::to_string(u1);
	}
}
BENCHMARK(BM_BoostUUIDToString);

BENCHMARK_MAIN();


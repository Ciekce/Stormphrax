/*
 * Polaris, a UCI chess engine
 * Copyright (C) 2023 Ciekce
 *
 * Polaris is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Polaris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Polaris. If not, see <https://www.gnu.org/licenses/>.
 */

#include "search.h"

#include <unordered_map>
#include <functional>
#include <vector>
#include <cctype>

#include "pvs/pvs_search.h"

namespace polaris::search
{
	namespace
	{
		using SearcherFunc = std::function<std::unique_ptr<search::ISearcher>(std::optional<size_t>)>;

		class Searchers
		{
		public:
			Searchers();
			~Searchers() = default;

			[[nodiscard]] inline std::span<const std::string> searchers() const
			{
				return m_names;
			}

			[[nodiscard]] inline std::optional<std::unique_ptr<ISearcher>> create(const std::string &name,
				std::optional<size_t> hashSize)
			{
				if (const auto itr = m_searchers.find(name); itr != m_searchers.end())
					return itr->second(hashSize);
				return {};
			}

		private:
			template <typename Searcher>
			inline void registerSearcher(std::string name)
			{
				m_names.emplace_back(name);

				std::transform(name.begin(), name.end(), name.begin(), [](auto c) { return std::tolower(c); });
				m_searchers.emplace(std::move(name), [](auto hashSize){ return std::make_unique<Searcher>(hashSize); });
			}

			std::unordered_map<std::string, SearcherFunc> m_searchers{};
			std::vector<std::string> m_names{};
		};

		Searchers::Searchers()
		{
			registerSearcher<pvs::PvsSearcher>("AspPVS");
		}

		Searchers s_searchers{};
	}

	std::span<const std::string> ISearcher::searchers()
	{
		return s_searchers.searchers();
	}

	std::unique_ptr<ISearcher> ISearcher::createDefault()
	{
		return std::make_unique<pvs::PvsSearcher>();
	}

	std::optional<std::unique_ptr<ISearcher>> ISearcher::create(const std::string &name,
		std::optional<size_t> hashSize)
	{
		return s_searchers.create(name, hashSize);
	}
}

#pragma once
#include <vector>
#include <memory>
#include "game/entity/StaticEntity.hpp"

class Field {
public:
    using EntityPtr = std::unique_ptr<StaticEntity>;

    // Accessors to enumerate entities
    const std::vector<EntityPtr> &entities() const { return m_statics; }
    std::vector<EntityPtr> &entities() { return m_statics; }

    // Management helpers
    void add(EntityPtr e) { m_statics.push_back(std::move(e)); }

    bool remove(const StaticEntity *e) {
        for (auto it = m_statics.begin(); it != m_statics.end(); ++it) {
            if (it->get() == e) {
                m_statics.erase(it);
                return true;
            }
        }
        return false;
    }

    void clear() { m_statics.clear(); }

private:
    std::vector<EntityPtr> m_statics;
};

// ============================================================================
//  Пример 03 — Връзки между обекти: `embed` срещу `reference`
// ============================================================================
//
//  Един и същ логически модел може да се материализира физически по два
//  начина в зависимост от системата за съхранение:
//
//    - `store_as::reference` --- връзка през идентификатор (foreign key в SQL,
//      key prefix в Redis, ребро в Neo4j). Свързаните данни се извличат с
//      допълнителна заявка / JOIN.
//
//    - `store_as::embed` --- свързаните данни се записват вътре в родителския
//      запис (вложен документ в MongoDB, денормализирано поле в Cassandra).
//
//  Това решение се описва ВЕДНЪЖ върху C++ модела; конекторът за конкретната
//  система за съхранение знае как да го материализира.
// ============================================================================

#include "ORM/ORM.hpp"

#include <iostream>
#include <vector>

// ── свързан обект ───────────────────────────────────────────────────────────
struct Post
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string,"body"> body;
};

// ── обект с reference връзка (нормализирана) ────────────────────────────────
struct UserNormalized
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string,"name"> name;
    // Postовете се пазят отделно, свързани чрез external id.
    orm::relationship<orm::store_as::reference, Post, "posts"> posts;
};

// ── обект с embed връзка (денормализирана) ──────────────────────────────────
struct UserDenormalized
{
    orm::property<int,          "id">   id;
    orm::property<std::u8string,"name"> name;
    // Постовете се записват директно в родителя --- естествено за MongoDB.
    orm::relationship<orm::store_as::embed, Post, "posts"> posts;
};

int main()
{
    using R_ref = decltype(UserNormalized::posts);
    using R_emb = decltype(UserDenormalized::posts);

    // ── статични свойства на връзките ─────────────────────────────────────
    static_assert(R_ref::strategy == orm::store_as::reference);
    static_assert(R_emb::strategy == orm::store_as::embed);

    // Изведени връзки от типа: cont (vector/list) → one-to-many.
    using PostList = std::vector<Post>;
    static_assert(orm::infer_relationship_v<PostList>
                  == orm::inferred_kind::one_to_many);
    static_assert(orm::infer_relationship_v<Post>
                  == orm::inferred_kind::one_to_one);

    std::cout << "UserNormalized: postовете се пазят отделно (reference)\n"
              << "  → в SQL: foreign key Post.user_id + JOIN\n"
              << "  → в Redis: ключ 'user:{id}:posts' с post IDs\n"
              << "  → в Neo4j: ребро (:User)-[:HAS]->(:Post)\n\n"
              << "UserDenormalized: постовете се вграждат (embed)\n"
              << "  → в MongoDB: вложен масив в документа\n"
              << "  → в Cassandra: денормализирана колона\n";
    return 0;
}

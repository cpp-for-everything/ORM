# <span style="font-size: 2.25em;">ORM Library for C++</span>
### Make your database usage easier and more universal now!
<hr>

#### Requirements
| Compiler version | Minimum C++ standard required |
|:----------------:|:-----------------------------:|
| GCC              | -std=c++20                    |

# How to use

1. Make sure to include the library and the database interface

    ```cpp
    #include <ORM/ORM.hpp>
    ```
<!---
    - MySQL
        ```cpp
        #include <ORM/database/MySQL/mysql.hpp>
        ```
    - MockSQL
        ```cpp
        #include <ORM/database/MockSQL/MockSQL.hpp>
        ```
1. Initiate your Database
    
    **_Note:_** Make sure your database is set as ``constexpr``.

    - MySQL
        ```cpp
        constexpr const auto DB = ORM::Database<ORM::MySQL>(DB_HOST, DB_USERNAME, DB_PASSWORD, DB_NAME, DB_PORT, UNIX_SOCKET/nullptr, MYSQL_FLAGS, AUTO_COMMIT);
        ``` 
    - MockSQL
        ```cpp
        constexpr const auto DB = ORM::Database<ORM::MockSQL>(RANDOM_STRING);
        ```
-->    
1. Initiate your table

    Any struct or class can be a table. Just use `webframe::ORM::property<type, column name>` to define the columns.    
    ```cpp
    struct Users {
        property<int, "id"> id;
        property<std::string, "username"> username;
        property<std::string, "password"> hashed_password;
    };
    ```
1. Initiate a relationship
   
    Any column can be used for a relationship. Just use `webframe::ORM::relationship<relationship type, mapping property>` to define the column.    
    ```cpp
    struct Post {
        property<int, "id"> id;
        property<std::string, "content"> content;
    };

    struct User {
        property<int, "id"> id;
        property<std::string, "username"> username;
        property<std::string, "password"> hashed_password;
        relationship<RelationshipTypes::one2many, &Post::id> posts;
    };
    ```
    - You can also create a separate mapping table:
    ```cpp
    struct UserPost {
        property<int, "id"> id;
        relationship<RelationshipTypes::one2one, &User::id> author;
        relationship<RelationshipTypes::one2one, &Post::id> post;
    };
    ```
1. Initiate your queries
   
    **_Note:_** The queries should be declared as ``constexpr``. For easy usage, they should be declared as ``static`` if declared in classes/structures.

    ```cpp
    struct UserPost {
        property<int, "id"> id;
        relationship<RelationshipTypes::one2one, &User::id> author;
        relationship<RelationshipTypes::one2one, &Post::id> post;

        static constexpr auto select_all = select(&UserPost::id, &User::id, &User::username, &Post::id, &Post::content)
                    .join((P<&UserPost::author> == P<&User::id>) && (P<&UserPost::post> == P<&Post::id>));
        // or
        static constexpr auto select_all = select(&UserPost::id, &User::id, &User::username, &Post::id, &Post::content)
                    .join(P<&UserPost::author> == P<&User::id>)
                    .join(P<&UserPost::post> == P<&Post::id>);

        static constexpr auto select_all_where_user_id_is_greater_than = select(&UserPost::id, &User::id, &User::username, &Post::id, &Post::content)
                    .join((P<&UserPost::author> == P<&User::id>) && (P<&UserPost::post> == P<&Post::id>))
                    .filter(P<&User::id> > Placeholder<int>);
    };
    ```
    or
    ```cpp
    constexpr auto select_all_users_and_posts = select(&UserPost::id, &User::id, &User::username, &Post::id, &Post::content)
                    .join((P<&UserPost::author> == P<&User::id>) && (P<&UserPost::post> == P<&Post::id>));
    //or
    constexpr auto select_all_users_and_posts = select(&UserPost::id, &User::id, &User::username, &Post::id, &Post::content)
                    .join(P<&UserPost::author> == P<&User::id>)
                    .join(P<&UserPost::post> == P<&Post::id>);
    constexpr auto select_all_where_user_id_is_greater_than = select(&UserPost::id, &User::id, &User::username, &Post::id, &Post::content)
                    .join((P<&UserPost::author> == P<&User::id>) && (P<&UserPost::post> == P<&Post::id>))
                    .filter(P<&User::id> > Placeholder<int>);
    ```
    _**Note:** We highly recommend using your own ``class`` or ``namespace`` where to put the relevant queries that you would like to use. If you want to prevent the use of some of the queries, make sure to use proper encapsulation._
1. How to use your queries
    - Request calls
        If the database and the table(s) are setup as shown above, the requests should be used in the following way:
        ```cpp
        auto collection = UserPost::select_all();
        //or
        auto collection = select_all_users_and_posts();
        auto collection = select_all_where_user_id_is_greater_than(5);
        ```
    - Access different columns of the output rows
        ```cpp
        for (auto row : collection) {
            std::cout << row.get<&User::id>() << ", ";
        }
        ```
    **_Note:_** Even if the request's output is 1 row, it is still returned as collection (std::list) of the rows.

Check [example/](https://github.com/WebFrame/ORM-Abstract/blob/main/example) for more information.

# Benefits of the library
<!--
1. DB migration
    If a change in the database type is needed, all you have to do is to change the ``DB`` variable template parameter with the new database. Then change the SQL requests if neccessary and you are set.
-->
1. Type-strict queries

    Webframe::ORM allows you to develop your queries as lambdas behind the scenes. This leads to later on passing values to those lambdas and it will fill the placeholders with them. However, it also checks the types of the parameters and compares them to those of the placeholders. If something doesn't match you get compile-time error which prevents bugs later.
1. Query optimization compile-time

    Webframe::ORM allows you to write your join conditions and filter conditions as boolean expression and it optimizes all of them compile-time for faster execution runtime.
1. SQL injection prevention

    All database interfaces have their own SQL injection protection in case you want to use query parameters. Webframe::ORM gives easy-to-use interface to the bridges between databases' API and the ORM which is meant to use the native API's SQL injection protection keeping you 100% safe from this kind of attack.

# Socials
[![LinkedIn](https://img.shields.io/badge/linkedin-%230077B5.svg?logo=linkedin&logoColor=white)](https://www.linkedin.com/in/alex-tsvetanov/)

# ToDo
1. Limits fix
    - ```sql
        [LIMIT [offset_value] number_rows | LIMIT number_rows OFFSET offset_value]
      ```
1. Result type
    - Member pointers and standard types
    - Get references by member pointer
    - Get references by table class
    - Get references by index
1. Rules
    - IN / NOT IN
1. CRUD operations
    - Read / select
      - COUNT(*) / COUNT(...)
      ```sql
        SELECT [ ALL | DISTINCT | DISTINCTROW ]
            [ HIGH_PRIORITY ]
            [ STRAIGHT_JOIN ]
            [ SQL_SMALL_RESULT | SQL_BIG_RESULT ] [ SQL_BUFFER_RESULT ]
            [ SQL_CACHE | SQL_NO_CACHE ]
            [ SQL_CALC_FOUND_ROWS ]
        expressions
        FROM tables
        [[LEFT | RIGHT | INNER] JOIN table]
        [WHERE conditions]
        [GROUP BY expressions]
        [HAVING condition]
        [ORDER BY expression [ ASC | DESC ]]
        [LIMIT [offset_value] number_rows | LIMIT number_rows OFFSET offset_value]
        INTO [ OUTFILE 'file_name' options 
            | DUMPFILE 'file_name'
            | @variable1, @variable2, ... @variable_n]
        [FOR UPDATE | LOCK IN SHARE MODE];
      ```
1. Transferring the abstract database implementation from v1.1 to v2 using the new SQL-free query style
1. Implement free MySQL driver
1. Implement free MongoDB driver
1. Auto-migrating on startup
    - Keep track of latest migration done
    - Apply new migrations if any
    - C++ Migrations tool
1. Run tests with SQL and NoSQL databases

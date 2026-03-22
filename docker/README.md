# Docker Testing Environment

This directory contains a comprehensive Docker-based testing environment for the ORM project. All database C++ connectors are installed at the system level, and tests run against live database instances within containers.

## Architecture

### Components

1. **Test Container** (`orm-test`): Ubuntu 24.04 with all DB client libraries installed
   - MySQL Connector/C++ 9.1.0
   - libpqxx 7.9.2 (PostgreSQL)
   - mongo-cxx-driver 4.0.0
   - hiredis 1.2.0 (Redis)
   - cassandra-cpp-driver 2.17.1
   - libneo4j-client 2.2.0
   - SQLite3

2. **Database Services**: Live database instances for integration testing
   - MySQL 8.4
   - PostgreSQL 16
   - MongoDB 7
   - Redis 7
   - Cassandra 4.1
   - Neo4j 5.25

### Build Strategy

The project supports two build strategies:

1. **System Libraries** (`ORM_USE_SYSTEM_LIBS=ON`): Uses `find_package()` to locate system-installed connectors
2. **Submodules** (`ORM_USE_SYSTEM_LIBS=OFF`, default): Builds connectors from git submodules

In the Docker environment, system libraries are always used.

## Usage

### Quick Start

Run all tests in Docker:

```bash
./docker/scripts/docker_test.sh
```

### Manual Testing

Start the environment:

```bash
docker compose -f docker/docker-compose.yml up -d
```

Build and enter the test container:

```bash
docker compose -f docker/docker-compose.yml run --rm orm-test bash
```

Inside the container, configure and build:

```bash
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DORM_USE_SYSTEM_LIBS=ON \
    -DORM_ENABLE_MYSQL=ON \
    -DORM_ENABLE_POSTGRESQL=ON \
    -DORM_ENABLE_MONGODB=ON \
    -DORM_ENABLE_REDIS=ON \
    -DORM_ENABLE_CASSANDRA=ON \
    -DORM_ENABLE_NEO4J=ON

cmake --build build --parallel $(nproc)
```

Run tests:

```bash
cd build
ctest --output-on-failure
```

Clean up:

```bash
docker compose -f docker/docker-compose.yml down -v
```

### Running Specific Tests

```bash
# Run only unit tests
docker compose -f docker/docker-compose.yml run --rm orm-test \
    /workspace/docker/scripts/run_tests.sh -R "^test_unit"

# Run only MySQL integration tests
docker compose -f docker/docker-compose.yml run --rm orm-test \
    /workspace/docker/scripts/run_tests.sh -R "IntegrationMySQL"

# Run with verbose output
docker compose -f docker/docker-compose.yml run --rm orm-test \
    /workspace/docker/scripts/run_tests.sh -V
```

## Database Connection Details

All databases are accessible from the host machine on non-standard ports:

| Database   | Host Port | Container Port | Credentials                          |
|------------|-----------|----------------|--------------------------------------|
| MySQL      | 3307      | 3306           | root / orm_test_password             |
| PostgreSQL | 5433      | 5432           | postgres / orm_test_password         |
| MongoDB    | 27018     | 27017          | (no auth)                            |
| Redis      | 6380      | 6379           | (no auth)                            |
| Cassandra  | 9043      | 9042           | (no auth)                            |
| Neo4j      | 7688      | 7687           | neo4j / orm_test_password            |

## CMake Find Modules

Custom CMake find modules are provided in `cmake/`:

- `FindMySQLConnectorCPP.cmake` - MySQL Connector/C++
- `FindLibPQXX.cmake` - PostgreSQL libpqxx
- `FindMongoCXX.cmake` - MongoDB C++ driver
- `FindHiredis.cmake` - Redis hiredis
- `FindCassandra.cmake` - Cassandra C++ driver
- `FindNeo4jClient.cmake` - Neo4j libneo4j-client

These modules create imported targets compatible with the submodule-based builds.

## CI Integration

The `.github/workflows/docker-tests.yml` workflow runs the full test suite in Docker on every push and pull request.

## Troubleshooting

```

## Development Workflow

1. Make code changes on host machine
2. Changes are immediately visible in container (via volume mount)
3. Rebuild and test inside container
4. No need to rebuild Docker image for code changes

## Performance Notes

- First build takes ~20-30 minutes (compiling all DB drivers)
- Subsequent builds are cached
- Database startup takes ~1-2 minutes (especially Cassandra)
- Test execution time depends on enabled connectors

## File Structure

```
docker/
├── Dockerfile.test          # Multi-stage build with all connectors
├── docker-compose.yml       # Service orchestration
├── scripts/
│   ├── run_tests.sh        # Test runner (runs inside container)
│   └── docker_test.sh      # Wrapper script (runs on host)
└── README.md               # This file
```

include(FetchContent)

FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v2.13.4)
FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(ApprovalTests
        GIT_REPOSITORY https://github.com/approvals/ApprovalTests.cpp.git
        GIT_TAG v.10.8.0)
FetchContent_MakeAvailable(ApprovalTests)

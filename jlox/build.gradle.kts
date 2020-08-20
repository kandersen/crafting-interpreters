version = "0.1"

plugins {
  java
  application
}

sourceSets {
  main {
    java {
      listOf("src")
    }
  }
}

application {
  mainClassName = "com.craftinginterpreters.lox.Lox"
}

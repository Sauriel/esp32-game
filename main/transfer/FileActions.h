#pragma once
namespace FileActions {
enum class Action { CreateDirectory, Delete, Move, Rename };
enum class Error { None, Invalid, NotFound, Conflict, Storage };
struct Result { Error error; const char* message; };
// Validierte SD-Pfade; ein serialisierter Worker besitzt alle Operationen.
Result execute(Action action, const char* source, const char* target = nullptr);
}

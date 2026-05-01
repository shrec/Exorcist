#pragma once

// ── DoxygenGenerator ─────────────────────────────────────────────────────────
//
// Utility that parses a (possibly multi-line) C++ function declaration or
// definition and produces a Doxygen `///`-style comment block with:
//   - @brief placeholder
//   - one @param line per parameter
//   - @return line for non-void / non-ctor / non-dtor functions
//
// Non-goals: this is a heuristic parser intended for the "Generate Doxygen
// Comment" editor command. It is intentionally tolerant — when in doubt it
// emits a minimal `///` line so the user is never blocked.
//
// Stateless. All methods are static. No Qt dependencies beyond QString.

#include <QList>
#include <QString>

class DoxygenGenerator
{
public:
    struct Param {
        QString type;          // e.g. "const QString &"
        QString name;          // e.g. "filePath"
        QString defaultValue;  // e.g. "QString()" — empty if none
    };

    struct ParsedSignature {
        QString      returnType;        // "" for ctor/dtor
        QString      name;              // "Foo" or "Foo::bar"
        QList<Param> params;
        bool         isConstructor = false;
        bool         isDestructor  = false;
        bool         isVoid        = false;
        bool         valid         = false;  // false => not a function-like line
    };

    // Parse a single function-line text (already joined across continuation
    // lines if needed). Returns ParsedSignature with valid=false if the line
    // is not a recognisable function declaration/definition.
    static ParsedSignature parseSignature(const QString &line);

    // Build a Doxygen `///`-style comment block from a parsed signature.
    // Each output line is prefixed with `indentSpaces` spaces so the comment
    // aligns with the function it describes.
    //
    // Output ends with a single trailing newline so the caller can insert it
    // immediately above the function line.
    static QString buildComment(const ParsedSignature &sig,
                                int indentSpaces = 0);

    // Convenience: trims/normalises a raw line and re-runs parseSignature.
    // Provided for tests and future use.
    static ParsedSignature parseLine(const QString &rawLine);
};

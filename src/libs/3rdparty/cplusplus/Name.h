// Copyright (c) 2008 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include "CPlusPlusForwardDeclarations.h"
#include "Matcher.h"

#include <functional>

namespace CPlusPlus {

class CPLUSPLUS_EXPORT Name
{
public:
    Name();
    virtual ~Name();

    virtual const Identifier *identifier() const = 0;

    virtual const Identifier *asNameId() const { return nullptr; }
    virtual const AnonymousNameId *asAnonymousNameId() const { return nullptr; }
    virtual const TemplateNameId *asTemplateNameId() const { return nullptr; }
    virtual const DestructorNameId *asDestructorNameId() const { return nullptr; }
    virtual const OperatorNameId *asOperatorNameId() const { return nullptr; }
    virtual const ConversionNameId *asConversionNameId() const { return nullptr; }
    virtual const QualifiedNameId *asQualifiedNameId() const { return nullptr; }
    virtual const SelectorNameId *asSelectorNameId() const { return nullptr; }

    void accept(NameVisitor *visitor) const;
    static void accept(const Name *name, NameVisitor *visitor);

    bool match(const Name *other, Matcher *matcher = nullptr) const;

public:
    struct Equals {
        bool operator()(const Name *name, const Name *other) const;
    };
    struct Hash {
        size_t operator()(const Name *name) const;
    };

protected:
    virtual void accept0(NameVisitor *visitor) const = 0;

protected: // for Matcher
    friend class Matcher;
    virtual bool match0(const Name *otherName, Matcher *matcher) const = 0;
};

} // namespace CPlusPlus

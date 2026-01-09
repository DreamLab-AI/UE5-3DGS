# Contributing to UE5-3DGS

Thank you for your interest in contributing to UE5-3DGS! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Pull Request Process](#pull-request-process)
- [Testing Guidelines](#testing-guidelines)
- [Documentation](#documentation)

---

## Code of Conduct

This project follows a standard code of conduct. Please be respectful and constructive in all interactions.

---

## Getting Started

### Prerequisites

- Unreal Engine 5.3 or later
- Visual Studio 2022 (Windows) or Xcode 14+ (macOS)
- Git

### Fork and Clone

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/YOUR_USERNAME/UE5-3DGS.git
cd UE5-3DGS

# Add upstream remote
git remote add upstream https://github.com/ORIGINAL_OWNER/UE5-3DGS.git
```

---

## Development Setup

### Project Structure

```
UE5-3DGS/
├── Source/
│   └── UE5_3DGS/
│       ├── Public/          # Public headers
│       │   ├── Core/        # Core systems
│       │   └── FCM/         # Format compliance
│       └── Private/         # Implementation
│           ├── Core/
│           └── FCM/
├── Tests/                   # Unit and integration tests
├── docs/                    # Documentation
└── README.md
```

### Building

```bash
# Generate project files (Windows)
GenerateProjectFiles.bat

# Build
MSBuild.exe YourProject.sln /p:Configuration=Development

# Or use Unreal Editor: Build → Build Solution
```

---

## Coding Standards

### UE5 Coding Conventions

Follow the [Epic C++ Coding Standard](https://docs.unrealengine.com/5.0/en-US/epic-cplusplus-coding-standard-for-unreal-engine/):

- **Prefixes**: `U` (UObject), `A` (Actor), `F` (struct/non-UObject), `E` (enum), `I` (interface)
- **Naming**: PascalCase for types and functions, no underscores
- **Braces**: Allman style (opening brace on new line)
- **Tabs**: Use tabs, not spaces

### Code Example

```cpp
/**
 * Converts position from UE5 to COLMAP coordinate system.
 * @param UE5Position Position in UE5 coordinates (cm, left-handed, Z-up)
 * @return Position in COLMAP coordinates (m, right-handed, Y-down)
 */
FVector FCoordinateConverter::UE5ToColmap(const FVector& UE5Position)
{
    return FVector(
        UE5Position.Y * ScaleFactor,
        -UE5Position.Z * ScaleFactor,
        UE5Position.X * ScaleFactor
    );
}
```

### Documentation

- Document all public APIs with `/** */` comments
- Include `@param` and `@return` tags
- Explain non-obvious behavior

---

## Pull Request Process

### Before Submitting

1. **Create a branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following coding standards

3. **Write/update tests** for new functionality

4. **Run all tests**:
   ```bash
   UE5Editor.exe YourProject -ExecCmds="Automation RunTests UE5_3DGS" -unattended
   ```

5. **Update documentation** if needed

6. **Commit with clear messages**:
   ```bash
   git commit -m "feat: Add spiral trajectory generation"
   ```

### Commit Message Format

Use [Conventional Commits](https://www.conventionalcommits.org/):

- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation only
- `test:` Adding tests
- `refactor:` Code refactoring
- `perf:` Performance improvement

### Submitting

1. Push your branch:
   ```bash
   git push origin feature/your-feature-name
   ```

2. Create a Pull Request on GitHub

3. Fill out the PR template with:
   - Description of changes
   - Related issues
   - Testing performed
   - Screenshots (if UI changes)

### Review Process

- PRs require at least one approval
- Address review feedback promptly
- Keep PRs focused and reasonably sized
- Squash commits before merging

---

## Testing Guidelines

### Test Categories

| Category | Location | Command |
|----------|----------|---------|
| Unit | `Tests/*Test.cpp` | `RunTests UE5_3DGS.Unit` |
| Integration | `Tests/Integration*.cpp` | `RunTests UE5_3DGS.Integration` |

### Writing Tests

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMyFeatureTest,
    "UE5_3DGS.Unit.MyFeature.TestCase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMyFeatureTest::RunTest(const FString& Parameters)
{
    // Arrange
    FMyClass Instance;

    // Act
    bool Result = Instance.DoSomething();

    // Assert
    TestTrue(TEXT("Should succeed"), Result);

    return true;
}
```

### Test Coverage

- All new public APIs must have tests
- Bug fixes should include regression tests
- Maintain existing test coverage

---

## Documentation

### When to Update Docs

- New public APIs
- Changed behavior
- New features
- Configuration options

### Documentation Files

| File | Content |
|------|---------|
| `README.md` | Project overview, quick start |
| `docs/api/README.md` | API reference |
| `docs/guides/user-guide.md` | Usage instructions |
| `docs/architecture/README.md` | System design |

### Style Guidelines

- Use Mermaid for diagrams
- Include code examples
- Keep language clear and concise
- Use collapsible sections for long content

---

## Questions?

- Open an issue for bugs or feature requests
- Use discussions for questions
- Tag maintainers for urgent issues

Thank you for contributing!

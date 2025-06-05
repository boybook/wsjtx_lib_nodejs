#!/bin/bash

# WSJTX-Lib GitHub Release Creation Script
# 
# 此脚本自动化创建GitHub Release并上传预构建包
# 使用Node.js标准平台名称：linux, darwin, win32

set -e

VERSION=$1
RELEASE_NOTES=$2

if [ -z "$VERSION" ]; then
    echo "❌ Error: Version is required"
    echo "Usage: $0 <version> [release_notes]"
    echo "Example: $0 v1.0.0 \"First stable release\""
    exit 1
fi

if [ -z "$RELEASE_NOTES" ]; then
    RELEASE_NOTES="Release $VERSION"
fi

echo "🚀 Creating GitHub Release: $VERSION"
echo "📝 Release notes: $RELEASE_NOTES"
echo ""

# 检查是否有预构建文件
PREBUILDS_DIR="prebuilds"
if [ ! -d "$PREBUILDS_DIR" ]; then
    echo "❌ Error: No prebuilds directory found!"
    echo "   Please download the GitHub Actions artifacts first."
    exit 1
fi

# 验证必要的平台（使用Node.js标准名称）
REQUIRED_PLATFORMS=("linux-x64" "darwin-arm64" "win32-x64")
MISSING_PLATFORMS=()

for platform in "${REQUIRED_PLATFORMS[@]}"; do
    platform_dir="$PREBUILDS_DIR/$platform"
    
    if [ ! -d "$platform_dir" ] || [ ! -f "$platform_dir/wsjtx_lib_nodejs.node" ]; then
        MISSING_PLATFORMS+=("$platform")
    fi
done

if [ ${#MISSING_PLATFORMS[@]} -gt 0 ]; then
    echo "⚠️  Warning: Missing prebuilds for platforms: ${MISSING_PLATFORMS[*]}"
    echo "   Continue anyway? (y/N)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# 创建临时目录用于打包
TEMP_DIR=$(mktemp -d)
echo "📁 Working in temporary directory: $TEMP_DIR"

# 打包函数
package_platform() {
    local platform=$1
    local source_dir=$2
    
    echo "📦 Packaging $platform..."
    
    # 创建平台特定的临时目录
    local platform_temp="$TEMP_DIR/$platform"
    mkdir -p "$platform_temp"
    
    # 复制所有文件到临时目录
    cp "$source_dir"/* "$platform_temp/"
    
    # 创建tar.gz包
    local package_name="wsjtx_lib_nodejs-$VERSION-$platform.tar.gz"
    cd "$platform_temp"
    tar -czf "$TEMP_DIR/$package_name" *
    cd - > /dev/null
    
    echo "   ✅ Created: $package_name"
    echo "      Size: $(du -h "$TEMP_DIR/$package_name" | cut -f1)"
    echo "      Files: $(tar -tzf "$TEMP_DIR/$package_name" | wc -l)"
}

# 打包所有平台
echo ""
echo "📦 Creating release packages..."

# 遍历所有预构建目录（现在应该使用标准平台名称）
for dir in "$PREBUILDS_DIR"/*; do
    if [ -d "$dir" ] && [ -f "$dir/wsjtx_lib_nodejs.node" ]; then
        dir_name=$(basename "$dir")
        package_platform "$dir_name" "$dir"
    fi
done

# 显示打包结果
echo ""
echo "📋 Release packages created:"
ls -lh "$TEMP_DIR"/*.tar.gz 2>/dev/null | while read -r line; do
    echo "   $line"
done

# 检查是否有生成的包
if [ ! -f "$TEMP_DIR"/*.tar.gz ]; then
    echo "❌ Error: No packages were created!"
    exit 1
fi

# 检查GitHub CLI是否可用
if ! command -v gh &> /dev/null; then
    echo ""
    echo "⚠️  GitHub CLI not found. Manual steps required:"
    echo "1. Create tag: git tag $VERSION && git push origin $VERSION"
    echo "2. Create release at: https://github.com/boybook/wsjtx_lib_nodejs/releases/new"
    echo "3. Upload these files:"
    ls "$TEMP_DIR"/*.tar.gz 2>/dev/null | while read -r file; do
        echo "   - $(basename "$file")"
    done
    echo "4. Files are located in: $TEMP_DIR"
    echo ""
    echo "Files will be kept in temp directory for manual upload."
    exit 0
fi

# 检查是否已登录GitHub CLI
if ! gh auth status &> /dev/null; then
    echo "❌ Error: Not logged in to GitHub CLI"
    echo "   Please run: gh auth login"
    exit 1
fi

# 创建或检查tag
echo ""
echo "🏷️  Checking tag $VERSION..."
if git rev-parse "$VERSION" >/dev/null 2>&1; then
    echo "   Tag $VERSION already exists"
else
    echo "   Creating tag $VERSION..."
    git tag "$VERSION"
    git push origin "$VERSION"
    echo "   ✅ Tag created and pushed"
fi

# 创建GitHub Release
echo ""
echo "🎯 Creating GitHub Release..."

gh release create "$VERSION" \
    --title "$VERSION" \
    --notes "$RELEASE_NOTES" \
    "$TEMP_DIR"/*.tar.gz

echo ""
echo "✅ Release $VERSION created successfully!"
echo "🔗 View at: https://github.com/boybook/wsjtx_lib_nodejs/releases/tag/$VERSION"

# 清理临时文件
echo ""
echo "🧹 Cleaning up temporary files..."
rm -rf "$TEMP_DIR"

echo "✅ Done!" 
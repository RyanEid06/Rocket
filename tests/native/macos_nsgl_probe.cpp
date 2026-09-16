#include <OpenGL/OpenGL.h>

#include <iostream>
#include <vector>

namespace {

bool choosePixelFormat(bool accelerated, bool offline) {
  std::vector<CGLPixelFormatAttribute> attributes;
  if (accelerated) attributes.push_back(kCGLPFAAccelerated);
  attributes.push_back(kCGLPFAClosestPolicy);
  if (offline) attributes.push_back(kCGLPFAAllowOfflineRenderers);
  attributes.push_back(kCGLPFAOpenGLProfile);
  attributes.push_back(
      static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core));
  attributes.push_back(kCGLPFAColorSize);
  attributes.push_back(static_cast<CGLPixelFormatAttribute>(24));
  attributes.push_back(kCGLPFAAlphaSize);
  attributes.push_back(static_cast<CGLPixelFormatAttribute>(8));
  attributes.push_back(kCGLPFADepthSize);
  attributes.push_back(static_cast<CGLPixelFormatAttribute>(24));
  attributes.push_back(kCGLPFAStencilSize);
  attributes.push_back(static_cast<CGLPixelFormatAttribute>(8));
  attributes.push_back(kCGLPFADoubleBuffer);
  attributes.push_back(static_cast<CGLPixelFormatAttribute>(0));

  CGLPixelFormatObj format = nullptr;
  GLint count = 0;
  const CGLError error =
      CGLChoosePixelFormat(attributes.data(), &format, &count);
  if (format != nullptr) CGLReleasePixelFormat(format);
  return error == kCGLNoError && count > 0;
}

}  // namespace

int main() {
  const bool accelerated = choosePixelFormat(true, false);
  const bool offline = choosePixelFormat(true, true);
  const bool software = choosePixelFormat(false, true);
  std::cout << "accelerated=" << accelerated << " offline=" << offline
            << " software=" << software << '\n';
  return (accelerated || offline || software) ? 0 : 1;
}

import Foundation
import WebKit
import SwiftUI

enum AmapWebSession {
    static func loadShare(_ text:String) async throws -> AmapShare.Route {
        let cookies:[HTTPCookie]=await withCheckedContinuation { continuation in
            DispatchQueue.main.async{WKWebsiteDataStore.default().httpCookieStore.getAllCookies{continuation.resume(returning:$0)}}
        }
        return try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.global(qos:.userInitiated).async {
                do {
                    let route=try AmapShare.load(text,cookies:cookies){updated in
                        DispatchQueue.main.async {
                            let store=WKWebsiteDataStore.default().httpCookieStore
                            for cookie in updated {store.setCookie(cookie)}
                        }
                    }
                    continuation.resume(returning:route)
                }catch{continuation.resume(throwing:error)}
            }
        }
    }
}

/// The user's normal web verification. No tokens or credentials are exported.
struct ShareVerificationView:View {
    let page:URL?
    let done:()->Void
    @Environment(\.dismiss) private var dismiss
    var body:some View {
        NavigationView {
            VStack(spacing:12){
                Text("在高德页面完成登录或验证，再点右上角返回重试。登录信息只保存在本机。").font(.caption).padding(.horizontal)
                ShareVerificationBrowser(page:page ?? URL(string:"https://www.amap.com/")!)
            }.navigationTitle("高德网页验证").toolbar{
                ToolbarItem(placement:.cancellationAction){Button("取消"){dismiss()}}
                ToolbarItem(placement:.confirmationAction){Button("完成并重试"){dismiss();done()}}
            }
        }
    }
}

private struct ShareVerificationBrowser:UIViewRepresentable {
    let page:URL
    func makeCoordinator()->Coordinator{Coordinator()}
    func makeUIView(context:Context)->WKWebView {
        let config=WKWebViewConfiguration();config.websiteDataStore = .default()
        let web=WKWebView(frame:.zero,configuration:config)
        web.customUserAgent=MotorRoutes.ua;web.navigationDelegate=context.coordinator
        web.load(URLRequest(url:page));return web
    }
    func updateUIView(_ view:WKWebView,context:Context){}
    final class Coordinator:NSObject,WKNavigationDelegate {
        func webView(_ webView:WKWebView,decidePolicyFor navigationAction:WKNavigationAction,decisionHandler:@escaping(WKNavigationActionPolicy)->Void){
            let scheme=navigationAction.request.url?.scheme?.lowercased()
            decisionHandler(scheme=="https" || scheme=="http" || scheme=="about" ? .allow:.cancel)
        }
    }
}

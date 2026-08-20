# JNI short-name exports bind to this exact Java package, class, and method set.
-keep class com.inferlab.edge.InferLabEdgeRuntime { *; }
-keep class com.inferlab.edge.InferLabEdgeRuntime$* { *; }

# Preserve application callback implementations invoked by native GetMethodID.
-keep class * implements com.inferlab.edge.InferLabEdgeRuntime$TokenCallback {
    boolean onToken(long, byte[]);
}

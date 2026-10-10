package com.halo.decomp;

public final class ReleaseVersionTest {
    public static void main(String[] args) {
        String[] valid = {"v0.1", "v0.1.0", "v0.1.1", "v0.9", "v0.10", "v1.0", "v999.999.999"};
        int[] expected = {1000, 1000, 1001, 9000, 10000, 1000000, 999999999};
        for (int i = 0; i < valid.length; ++i)
            if (ReleaseVersion.code(valid[i]) != expected[i]) throw new AssertionError(valid[i]);
        String[] invalid = {null, "", "build-168", "v.0.1", "v1", "v01.2", "v1.02", "v1.2.03",
            "v1.2.", "v1.2.3.4", "v1.2-beta", "v1.2+abc", "v1.2/evil", "v1.2 ", "v1000.0", "v-1.0"};
        for (String tag : invalid)
            if (ReleaseVersion.code(tag) != -1) throw new AssertionError(tag);
        System.out.println("GulchCE Android release version checks passed");
    }
}

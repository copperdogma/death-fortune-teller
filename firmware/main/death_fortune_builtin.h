#pragma once
// Exact verified card template (727 bytes), retained for unreliable SD reads.
// SHA256: 50d46c691e8dbdf6808a2321467f4e62861167a5f1748fec11c73c5cc2d24661
namespace skull::fortune::builtin {
inline constexpr char json[] = R"DEATH_FORTUNE({
  "version": 1,
  "templates": [
    "Beware the {{1}} with the {{2}} {{3}}.Seek ye {{4}} for there you will {{5}} a {{6}}.On the dawn of the {{7}} {{8}}, you shall be {{9}} so that you may not {{10}} at all!"
  ],
  "wordlists": {
    "1": ["man", "woman", "donkey"],
    "2": ["blue", "fluffy", "edible"],
    "3": ["trousers", "earmuffs", "eyebrows"],
    "4": ["a red carriage", "a wooded hill", "a duck"],
    "5": ["encounter", "stumble upon", "speak with"],
    "6": ["strange wise man", "expired clown", "majestic frog"],
    "7": ["sixteenth", "thirteenth", "third"],
    "8": ["night", "dawn", "cow"],
    "9": ["blessed", "cursed", "mildly constipated"],
    "10": ["clean", "have candy", "look at nostrils"]
  }
})DEATH_FORTUNE";
static_assert(sizeof(json)-1==727, "Verified card template length changed");
}

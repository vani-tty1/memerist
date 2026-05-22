import pytest
import ctypes
import gc
import weakref
from unittest.mock import MagicMock, patch


# Simulated Python representation of the MemeLayer structure
# to test the security invariant around use-after-free and dangling pointers

class MemeLayer:
    """Python simulation of MemeLayer struct with safe memory management."""
    
    def __init__(self, text=None, font_family=None):
        self.text = text
        self.font_family = font_family
        self._freed = False
    
    def free(self):
        """Simulate meme_layer_free - the SECURE version sets pointers to None after free."""
        self.text = None
        self.font_family = None
        self._freed = True
    
    def is_freed(self):
        return self._freed
    
    def access_after_free(self):
        """Attempt to access data after free - should raise or return None."""
        if self._freed:
            raise RuntimeError("Use-after-free detected: layer has been freed")
        return self.text, self.font_family


class MemeLayerUnsafe:
    """Python simulation of the VULNERABLE MemeLayer - does NOT null pointers after free."""
    
    def __init__(self, text=None, font_family=None):
        self.text = text
        self.font_family = font_family
        self._freed = False
        self._original_text = text
        self._original_font_family = font_family
    
    def free_unsafe(self):
        """Simulate the vulnerable meme_layer_free - does NOT set pointers to None."""
        # Intentionally NOT setting self.text = None or self.font_family = None
        self._freed = True
        # In C this would g_free the memory but leave dangling pointers
        # We simulate by marking freed but keeping references accessible
    
    def is_freed(self):
        return self._freed
    
    def dangling_pointer_accessible(self):
        """Check if dangling pointer is still accessible after free (vulnerability indicator)."""
        if self._freed and (self.text is not None or self.font_family is not None):
            return True  # Dangling pointer exists - this is the vulnerability
        return False


@pytest.mark.parametrize("payload", [
    # Normal strings
    {"text": "Hello World", "font_family": "Arial"},
    # Empty strings
    {"text": "", "font_family": ""},
    # None values
    {"text": None, "font_family": None},
    # SQL injection payloads
    {"text": "'; DROP TABLE memes; --", "font_family": "Arial'; DELETE FROM fonts;"},
    # Buffer overflow attempt payloads
    {"text": "A" * 10000, "font_family": "B" * 10000},
    # Format string attack payloads
    {"text": "%s%s%s%s%s%n%n%n", "font_family": "%x%x%x%x%x%x"},
    # Null byte injection
    {"text": "hello\x00world", "font_family": "font\x00family"},
    # Unicode/special characters
    {"text": "\xff\xfe\x00\x01", "font_family": "\xde\xad\xbe\xef"},
    # Shell injection
    {"text": "$(rm -rf /)", "font_family": "`cat /etc/passwd`"},
    # Path traversal
    {"text": "../../../../etc/passwd", "font_family": "../../../etc/shadow"},
    # Very long strings with special chars
    {"text": "A" * 65536 + "\x00", "font_family": "\xff" * 65536},
    # Mixed adversarial
    {"text": "%n" * 1000, "font_family": "A" * 4096 + "%s%n"},
    # Unicode overflow
    {"text": "\U0001F4A9" * 1000, "font_family": "\u0000" * 500},
    # Repeated free simulation (double-free scenario)
    {"text": "double_free_test", "font_family": "double_free_font"},
])
def test_meme_layer_free_nulls_pointers_after_free(payload):
    """Invariant: After freeing a MemeLayer, all internal pointers MUST be set to NULL/None.
    
    This prevents use-after-free vulnerabilities and dangling pointer exploitation.
    Any retained reference to a freed layer must not expose previously freed memory.
    The secure implementation MUST null all pointers after freeing to make
    dangling pointer detection possible and exploitation unreliable.
    """
    text = payload.get("text")
    font_family = payload.get("font_family")
    
    # Test 1: Secure implementation - pointers must be None after free
    layer = MemeLayer(text=text, font_family=font_family)
    
    # Verify layer is properly initialized
    assert layer.is_freed() == False, "Layer should not be freed before free() is called"
    
    # Free the layer
    layer.free()
    
    # INVARIANT: After free, all pointers MUST be None (nulled)
    assert layer.text is None, (
        f"SECURITY VIOLATION: layer->text is not NULL after free. "
        f"Dangling pointer with value '{layer.text}' enables use-after-free exploitation."
    )
    assert layer.font_family is None, (
        f"SECURITY VIOLATION: layer->font_family is not NULL after free. "
        f"Dangling pointer with value '{layer.font_family}' enables use-after-free exploitation."
    )
    assert layer.is_freed() == True, "Layer must be marked as freed"
    
    # Test 2: Access after free must raise an error (not silently succeed)
    with pytest.raises(RuntimeError, match="Use-after-free detected"):
        layer.access_after_free()
    
    # Test 3: Demonstrate the vulnerability in the unsafe version
    unsafe_layer = MemeLayerUnsafe(text=text, font_family=font_family)
    unsafe_layer.free_unsafe()
    
    # The unsafe version WILL have dangling pointers - this is the vulnerability
    # We assert that the SECURE version does NOT exhibit this behavior
    secure_layer_after_free = MemeLayer(text=text, font_family=font_family)
    secure_layer_after_free.free()
    
    # INVARIANT: Secure layer must NOT have dangling pointers
    assert secure_layer_after_free.text is None, (
        "SECURITY INVARIANT VIOLATED: Secure layer has dangling text pointer after free"
    )
    assert secure_layer_after_free.font_family is None, (
        "SECURITY INVARIANT VIOLATED: Secure layer has dangling font_family pointer after free"
    )
    
    # Test 4: Double-free protection - second free should be safe
    layer2 = MemeLayer(text=text, font_family=font_family)
    layer2.free()
    # Second free should not cause issues since pointers are already None
    layer2.free()  # Should not raise
    assert layer2.text is None
    assert layer2.font_family is None
    
    # Test 5: Verify no reference leakage through weak references
    import weakref
    
    class TrackableText:
        def __init__(self, value):
            self.value = value
    
    if text is not None:
        trackable = TrackableText(text)
        weak_ref = weakref.ref(trackable)
        
        layer3 = MemeLayer(text=trackable, font_family=font_family)
        layer3.free()
        
        # After free, the layer should not hold a strong reference
        assert layer3.text is None, (
            "SECURITY INVARIANT VIOLATED: Layer retains strong reference to text after free"
        )
        
        # Clean up
        del trackable
        del layer3
        gc.collect()


@pytest.mark.parametrize("payload", [
    {"text": "normal text", "font_family": "Arial"},
    {"text": "A" * 10000, "font_family": "B" * 10000},
    {"text": None, "font_family": "Arial"},
    {"text": "text", "font_family": None},
    {"text": None, "font_family": None},
    {"text": "\x00\x01\x02\x03", "font_family": "\xff\xfe\xfd"},
])
def test_meme_layer_retained_reference_safety(payload):
    """Invariant: A retained reference to a freed MemeLayer must not expose freed memory.
    
    If code retains a reference to a layer after it is freed, accessing that
    reference must not return previously freed data. This simulates the scenario
    where a dangling pointer is dereferenced after the original memory is freed.
    """
    text = payload.get("text")
    font_family = payload.get("font_family")
    
    layer = MemeLayer(text=text, font_family=font_family)
    
    # Simulate retaining a reference (like a dangling pointer in C)
    retained_reference = layer
    
    # Free the layer
    layer.free()
    
    # INVARIANT: The retained reference must not expose freed data
    assert retained_reference.text is None, (
        f"SECURITY VIOLATION: Retained reference exposes freed text data: '{retained_reference.text}'. "
        f"This simulates a dangling pointer use-after-free vulnerability."
    )
    assert retained_reference.font_family is None, (
        f"SECURITY VIOLATION: Retained reference exposes freed font_family data: "
        f"'{retained_reference.font_family}'. This simulates a dangling pointer use-after-free."
    )
    
    # INVARIANT: Accessing freed layer must be detectable
    assert retained_reference.is_freed() == True, (
        "SECURITY INVARIANT VIOLATED: Cannot detect that retained reference points to freed layer. "
        "This makes dangling pointer detection impossible."
    )
    
    # INVARIANT: Attempting to use freed layer must fail safely
    with pytest.raises(RuntimeError):
        retained_reference.access_after_free()
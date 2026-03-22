function isEditableElement(element) {
    if (!element) {
        return false;
    }

    if (element.isContentEditable) {
        return true;
    }

    return ['INPUT', 'TEXTAREA', 'SELECT'].includes(element.tagName);
}

document.addEventListener('keydown', (event) => {
    if (event.defaultPrevented || event.repeat) {
        return;
    }

    if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) {
        return;
    }

    if (isEditableElement(document.activeElement)) {
        return;
    }

    const searchInput = document.querySelector('.wy-side-nav-search input[name="q"]');
    if (!searchInput) {
        return;
    }

    if (event.key === 's' || event.key === '/') {
        event.preventDefault();
        searchInput.focus();
        searchInput.select();
    }
});
